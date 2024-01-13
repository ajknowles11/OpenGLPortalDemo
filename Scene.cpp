#include "Scene.hpp"

#include "gl_errors.hpp"
#include "read_write_chunk.hpp"
#include "load_save_png.hpp"

#include "ColorTextureProgram.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <fstream>

//-------------------------

glm::mat4x3 Scene::Transform::make_local_to_parent() const {
	//compute:
	//   translate   *   rotate    *   scale
	// [ 1 0 0 p.x ]   [       0 ]   [ s.x 0 0 0 ]
	// [ 0 1 0 p.y ] * [ rot   0 ] * [ 0 s.y 0 0 ]
	// [ 0 0 1 p.z ]   [       0 ]   [ 0 0 s.z 0 ]
	//                 [ 0 0 0 1 ]   [ 0 0   0 1 ]

	glm::mat3 rot = glm::mat3_cast(rotation);
	return glm::mat4x3(
		rot[0] * scale.x, //scaling the columns here means that scale happens before rotation
		rot[1] * scale.y,
		rot[2] * scale.z,
		position
	);
}

glm::mat4x3 Scene::Transform::make_parent_to_local() const {
	//compute:
	//   1/scale       *    rot^-1   *  translate^-1
	// [ 1/s.x 0 0 0 ]   [       0 ]   [ 0 0 0 -p.x ]
	// [ 0 1/s.y 0 0 ] * [rot^-1 0 ] * [ 0 0 0 -p.y ]
	// [ 0 0 1/s.z 0 ]   [       0 ]   [ 0 0 0 -p.z ]
	//                   [ 0 0 0 1 ]   [ 0 0 0  1   ]

	glm::vec3 inv_scale;
	//taking some care so that we don't end up with NaN's , just a degenerate matrix, if scale is zero:
	inv_scale.x = (scale.x == 0.0f ? 0.0f : 1.0f / scale.x);
	inv_scale.y = (scale.y == 0.0f ? 0.0f : 1.0f / scale.y);
	inv_scale.z = (scale.z == 0.0f ? 0.0f : 1.0f / scale.z);

	//compute inverse of rotation:
	glm::mat3 inv_rot = glm::mat3_cast(glm::inverse(rotation));

	//scale the rows of rot:
	inv_rot[0] *= inv_scale;
	inv_rot[1] *= inv_scale;
	inv_rot[2] *= inv_scale;

	return glm::mat4x3(
		inv_rot[0],
		inv_rot[1],
		inv_rot[2],
		inv_rot * -position
	);
}

glm::mat4x3 Scene::Transform::make_local_to_world() const {
	if (!parent) {
		return make_local_to_parent();
	} else {
		return parent->make_local_to_world() * glm::mat4(make_local_to_parent()); //note: glm::mat4(glm::mat4x3) pads with a (0,0,0,1) row
	}
}
glm::mat4x3 Scene::Transform::make_world_to_local() const {
	if (!parent) {
		return make_parent_to_local();
	} else {
		return make_parent_to_local() * glm::mat4(parent->make_world_to_local()); //note: glm::mat4(glm::mat4x3) pads with a (0,0,0,1) row
	}
}

//-------------------------

void Scene::Transform::make_global(Transform const &from) {
	glm::mat4x3 const &from_to_world = from.make_local_to_world();

	position = from_to_world[3];
	scale = glm::vec3(glm::length(glm::vec3(from_to_world[0])), glm::length(glm::vec3(from_to_world[1])), glm::length(glm::vec3(from_to_world[2])));
	rotation = glm::quat_cast(glm::mat3(glm::vec3(from_to_world[0]) / scale[0], glm::vec3(from_to_world[1]) / scale[1], glm::vec3(from_to_world[2]) / scale[2]));
}

//-------------------------

glm::mat4 Scene::Camera::make_projection() const {
	return glm::infinitePerspective( fovy, aspect, near );
}

//-------------------------

// gets clipping plane in the middle of this portal, facing away from view_pos
glm::vec4 Scene::Portal::get_clipping_plane(glm::vec3 view_pos) const {
	glm::mat4x3 const p_world = drawable->transform->make_local_to_world();
	glm::vec3 p_forward = glm::normalize(p_world[1]);
	glm::vec3 const p_origin = glm::vec3(p_world * glm::vec4(0,0,0,1));
	glm::vec3 const camera_offset_from_portal = view_pos - p_origin;
	if (glm::dot(p_forward, camera_offset_from_portal) >= 0) p_forward *= -1;

	return glm::vec4(p_forward, -glm::dot(p_origin, p_forward));
}

//-------------------------

void Scene::draw(Camera const &camera) const {
	assert(camera.transform);
	glm::mat4x3 const cam_to_world = camera.transform->make_local_to_world();
	glm::vec4 const clip_plane = glm::vec4(-cam_to_world[2], 
		-glm::dot(cam_to_world * glm::vec4(0,0,0,1), -cam_to_world[2]));
	Transform cam_transform = Transform();
	cam_transform.make_global(*camera.transform);

	draw(camera.make_projection(), cam_transform, clip_plane, default_draw_recursion_max);
}

// https://th0mas.nl/2013/05/19/rendering-recursive-portals-with-opengl/
// https://github.com/ThomasRinsma/opengl-game-test/blob/8363bbf/src/scene.cc
void Scene::draw(glm::mat4 const &cam_projection, Transform const &cam_transform, glm::vec4 const &clip_plane, GLint max_recursion_lvl, GLint recursion_lvl, Portal const *from) const {

	//Calculate world_to_clip and world_to_light matrices for this case
	glm::mat4 const &world_to_clip = cam_projection * glm::mat4(cam_transform.make_world_to_local());
	static glm::mat4x3 const &world_to_light = glm::mat4x3(1.0f);

	if (from == nullptr && current_group == nullptr) {
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glDepthMask(GL_TRUE);
		glStencilMask(0x00);
		glEnable(GL_DEPTH_TEST);
		glDisable(GL_STENCIL_TEST);
		draw_non_portals(world_to_clip, world_to_light, true, clip_plane);
		return; //probably shouldn't happen, but maybe we'll want sometimes
	}

	std::vector<Portal*> const &local_portals = from == nullptr ? *current_group : portal_groups.at(from->group);
	
	for (auto const *p : local_portals) {
		if (p == from) continue;
		if (p->dest == nullptr) continue;
		if (!p->active) continue;
		if (!is_portal_visible(world_to_clip, *p)) continue;

		glm::vec4 const &p_clip_plane = p->get_clipping_plane(cam_transform.position);

		// Disable color and depth drawing
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		glDepthMask(GL_FALSE);

		// Enable depth test
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);

		// Enable stencil test, to prevent drawing outside
		// region of current portal depth
		glEnable(GL_STENCIL_TEST);

		// Pass stencil test when inside of outer portal
		// (pass where we should be drawing the inner portal)
		glStencilFunc(GL_EQUAL, recursion_lvl, 0xFF);

		// Increment stencil value on stencil and depth pass
		// (on area of inner portal)
		glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);

		// Enable (writing into) all stencil bits
		glStencilMask(0xFF);

		// Draw portal into stencil buffer
		draw_one(*p->drawable, world_to_clip, world_to_light, 2, clip_plane, p_clip_plane);

		// Enable color and enable depth drawing
		// (We enable color because draw_fullscreen_tri will also set the inside of portal to the clear color)
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glDepthMask(GL_TRUE);

		// Enable stencil test
		// So we can limit drawing inside of the inner portal
		glEnable(GL_STENCIL_TEST);

		// Disable drawing into stencil buffer
		glStencilMask(0x00);

		// Draw only where stencil value == recursionLevel + 1
		// which is where we just drew the new portal
		glStencilFunc(GL_EQUAL, recursion_lvl + 1, 0xFF);

		// Enable the depth test
		// So the stuff we render here is rendered correctly
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_ALWAYS);

		//https://stackoverflow.com/questions/38287235/opengl-how-to-implement-portal-rendering
		// Now set depth range to (1,1), leaving a "hole" for new objects through the portal.
		// This way we effectively clear depth only inside the portal.
		glDepthRange(1, 1);
		draw_fullscreen_tri();

		// Cleanup from depth clear hack
		glDepthRange(0, 1);
		glDepthFunc(GL_LESS);


		// Calculate new camera transform as if player was already teleported
		glm::mat4 const &portal_dest_mat = glm::mat4(p->dest->drawable->transform->make_local_to_world()) * glm::mat4(p->drawable->transform->make_world_to_local());
		Transform const &new_cam_transform = Transform(portal_dest_mat * glm::vec4(cam_transform.position, 1), 
			portal_dest_mat * glm::mat4(cam_transform.rotation), cam_transform.scale);
		glm::mat4 const &new_world_to_clip = cam_projection * glm::mat4(new_cam_transform.make_world_to_local());

		if (recursion_lvl == max_recursion_lvl) {
			// Base case, render inside of inner portal

			// Draw scene objects with destView, limited to stencil buffer
			// use an edited projection matrix to set the near plane to the portal plane
			draw_non_portals(new_world_to_clip, world_to_light, true, p->dest->get_clipping_plane(new_cam_transform.position));
		}
		else {
			// Recursion case

			// Pass our new view matrix and the clipped projection matrix (see above)
			draw(cam_projection, new_cam_transform, p->dest->get_clipping_plane(new_cam_transform.position), max_recursion_lvl, recursion_lvl + 1, p->dest);
		}

		// Disable color drawing
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		
		// Enable depth drawing and set depth to always pass
		glDepthMask(GL_TRUE);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_ALWAYS);
		
		// Enable stencil test, pass inside new portal. 
		glEnable(GL_STENCIL_TEST);
		glStencilFunc(GL_EQUAL, recursion_lvl + 1, 0xFF);

		// Enable stencil drawing
		glStencilMask(0xFF);

		// Decrement stencil value on stencil pass
		// This resets the incremented values to what they were before,
		// eventually ending up with a stencil buffer full of zero's again
		// after the last (outer) step.
		glStencilOp(GL_KEEP, GL_KEEP, GL_DECR);
		
		// Draw portal into depth and stencil buffer
		draw_one(*p->drawable, world_to_clip, world_to_light, 2, clip_plane, p_clip_plane);

		// Reset depth func to less
		glDepthFunc(GL_LESS);
	}

	// Draw at stencil >= recursionlevel
	// which is at the current level or higher (more to the inside)
	// This basically prevents drawing on the outside of this level.
	glStencilFunc(GL_LEQUAL, recursion_lvl, 0xFF);

	// Enable color and depth drawing again
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDepthMask(GL_TRUE);
	glStencilMask(0x00);

	// And enable the depth test
	glEnable(GL_DEPTH_TEST);

	// Draw scene objects normally, only at recursionLevel
	draw_non_portals(world_to_clip, world_to_light, true, clip_plane);
}

void Scene::draw_non_portals(glm::mat4 const &world_to_clip, glm::mat4x3 const &world_to_light, bool const &use_clip, glm::vec4 const &clip_plane) const {
	for (auto const &drawable : drawables) {
		draw_one(drawable, world_to_clip, world_to_light, use_clip, clip_plane);
	}
}

void Scene::draw_one(Drawable const &drawable, glm::mat4 const &world_to_clip, glm::mat4x3 const &world_to_light, uint8_t const &clip_plane_count, glm::vec4 const &clip_plane, glm::vec4 const &self_clip_plane) const {
	//Reference to drawable's pipeline for convenience:
	Scene::Drawable::Pipeline const &pipeline = drawable.pipeline;

	//skip any drawables without a shader program set:
	if (pipeline.program == 0) return;
	//skip any drawables that don't reference any vertex array:
	if (pipeline.vao == 0) return;
	//skip any drawables that don't contain any vertices:
	if (pipeline.count == 0) return;

	if (clip_plane_count > 1){
		glEnable(GL_CLIP_DISTANCE1);
		glEnable(GL_CLIP_DISTANCE0);
	}
	else if (clip_plane_count > 0) {
		glEnable(GL_CLIP_DISTANCE0);
	}


	//Set shader program:
	glUseProgram(pipeline.program);

	//Set attribute sources:
	glBindVertexArray(pipeline.vao);

	//Configure program uniforms:

	//the object-to-world matrix is used in all three of these uniforms:
	assert(drawable.transform); //drawables *must* have a transform
	glm::mat4x3 object_to_world = drawable.transform->make_local_to_world();

	//OBJECT_TO_CLIP takes vertices from object space to clip space:
	if (pipeline.OBJECT_TO_CLIP_mat4 != -1U) {
		glm::mat4 object_to_clip = world_to_clip * glm::mat4(object_to_world);
		glUniformMatrix4fv(pipeline.OBJECT_TO_CLIP_mat4, 1, GL_FALSE, glm::value_ptr(object_to_clip));
	}

	//the object-to-light matrix is used in the next two uniforms:
	glm::mat4x3 object_to_light = world_to_light * glm::mat4(object_to_world);

	//OBJECT_TO_CLIP takes vertices from object space to light space:
	if (pipeline.OBJECT_TO_LIGHT_mat4x3 != -1U) {
		glUniformMatrix4x3fv(pipeline.OBJECT_TO_LIGHT_mat4x3, 1, GL_FALSE, glm::value_ptr(object_to_light));
	}

	//NORMAL_TO_CLIP takes normals from object space to light space:
	if (pipeline.NORMAL_TO_LIGHT_mat3 != -1U) {
		glm::mat3 normal_to_light = glm::inverse(glm::transpose(glm::mat3(object_to_light)));
		glUniformMatrix3fv(pipeline.NORMAL_TO_LIGHT_mat3, 1, GL_FALSE, glm::value_ptr(normal_to_light));
	}

	if (pipeline.CLIP_PLANE_vec4 != -1U) {
		glUniform4fv(pipeline.CLIP_PLANE_vec4, 1, glm::value_ptr(clip_plane));
	}

	if (pipeline.SELF_CLIP_PLANE_vec4 != -1U) {
		glUniform4fv(pipeline.SELF_CLIP_PLANE_vec4, 1, glm::value_ptr(self_clip_plane));
	}

	//set any requested custom uniforms:
	if (pipeline.set_uniforms) pipeline.set_uniforms();

	//set up textures:
	for (uint32_t i = 0; i < Drawable::Pipeline::TextureCount; ++i) {
		if (pipeline.textures[i].texture != 0) {
			glActiveTexture(GL_TEXTURE0 + i);
			glBindTexture(pipeline.textures[i].target, pipeline.textures[i].texture);
		}
	}

	//draw the object:
	glDrawArrays(pipeline.type, pipeline.start, pipeline.count);

	//un-bind textures:
	for (uint32_t i = 0; i < Drawable::Pipeline::TextureCount; ++i) {
		if (pipeline.textures[i].texture != 0) {
			glActiveTexture(GL_TEXTURE0 + i);
			glBindTexture(pipeline.textures[i].target, 0);
		}
	}
	glActiveTexture(GL_TEXTURE0);

	glUseProgram(0);
	glBindVertexArray(0);

	if (clip_plane_count > 1){
		glDisable(GL_CLIP_DISTANCE1);
		glDisable(GL_CLIP_DISTANCE0);
	}
	else if (clip_plane_count > 0) {
		glDisable(GL_CLIP_DISTANCE0);
	}

	GL_ERRORS();
}

void Scene::draw_fullscreen_tri() const {
	if (full_tri_program.program == 0) {
		return;
	}
	glUseProgram(full_tri_program.program);
	glBindVertexArray(full_tri_program.vao);

	//set shader to draw clear color
	GLfloat clear_color[4];
	glGetFloatv(GL_COLOR_CLEAR_VALUE, clear_color);
	glUniform4fv(full_tri_program.CLEAR_COLOR_vec4, 1, clear_color);

	glDrawArrays(GL_TRIANGLES, 0, 3);
	glUseProgram(0);
	glBindVertexArray(0);

	GL_ERRORS();
}

bool Scene::is_portal_visible(glm::mat4 const &world_to_clip, Portal const &portal) const {
	glm::mat4 const &portal_to_clip = world_to_clip * glm::mat4(portal.drawable->transform->make_local_to_world());

	if (portal_to_clip[3][3] == 0) return false;
	// https://bruop.github.io/frustum_culling/

	// Get all verts of bbox, in clip space
	glm::vec4 const vertices[8] = {
		portal_to_clip * glm::vec4(portal.tp_box.min, 1), 
		portal_to_clip * glm::vec4(portal.tp_box.min.x, portal.tp_box.min.y, portal.tp_box.max.z, 1), 
		portal_to_clip * glm::vec4(portal.tp_box.min.x, portal.tp_box.max.y, portal.tp_box.min.z, 1), 
		portal_to_clip * glm::vec4(portal.tp_box.min.x, portal.tp_box.max.y, portal.tp_box.max.z, 1), 
		portal_to_clip * glm::vec4(portal.tp_box.max.x, portal.tp_box.min.y, portal.tp_box.min.z, 1), 
		portal_to_clip * glm::vec4(portal.tp_box.max.x, portal.tp_box.min.y, portal.tp_box.max.z, 1), 
		portal_to_clip * glm::vec4(portal.tp_box.max.x, portal.tp_box.max.y, portal.tp_box.min.z, 1), 
		portal_to_clip * glm::vec4(portal.tp_box.max, 1)
	};

	// Check if all verts lie outside any one plane
	bool outside_plane = true;
	// x, y
	for (uint8_t i = 0; i < 2; i++) {
		for (auto const &vert : vertices) {
			if (-vert.w <= vert[i]) {
				outside_plane = false;
				break;
			}
		}
		if (outside_plane) {
			return false;
		}
		for (auto const &vert : vertices) {
			if (vert.w >= vert[i]) {
				outside_plane = false;
				break;
			}
		}
		if (outside_plane) {
			return false;
		}
	}
	// z
	for (auto const &vert : vertices) {
		if (0 <= vert.w) {
			return true;
		}
	}

	return false;
}

Scene::Texture::Texture(std::string const &filename) {
    load_png(filename, &size, &pixels, LowerLeftOrigin);
}

Scene::ScreenImage::ScreenImage(Texture texture, glm::vec2 origin_, glm::vec2 size_, OriginMode origin_mode_, ColorTextureProgram const *program_) 
	: origin(origin_), size(size_), origin_mode(origin_mode_), program(program_) {
	// pass texture data
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture.size.x, texture.size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, texture.pixels.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glBindTexture(GL_TEXTURE_2D, 0);

	// buffer
	glGenBuffers(1, &buffer);

	// vao
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, buffer);
	glVertexAttribPointer(
		program->Position_vec4, 
		3, 
		GL_FLOAT, 
		GL_FALSE, 
		sizeof(Vert), 
		(GLbyte *)0 + offsetof(Vert, position)
	);
	glEnableVertexAttribArray(program->Position_vec4);

	glVertexAttribPointer(
		program->TexCoord_vec2, 
		2, 
		GL_FLOAT, 
		GL_FALSE, 
		sizeof(Vert), 
		(GLbyte *)0 + offsetof(Vert, tex_coord)
	);
	glEnableVertexAttribArray(program->TexCoord_vec2);

	glVertexAttribPointer(
			program->Color_vec4, //attribute
			4, //size
			GL_FLOAT, //type
			GL_FALSE, //normalized
			sizeof(Vert), //stride
			(GLbyte *)0 + offsetof(Vert, color) //offset
	);
	glEnableVertexAttribArray(program->Color_vec4);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

void Scene::ScreenImage::draw(float aspect) {
	std::vector< Vert > attribs;

	if (origin_mode == Center) {
		attribs.emplace_back(glm::vec3(origin.x - (size.x * 0.5f), origin.y - aspect * (size.y * 0.5f), 0.0f), glm::vec2(0.0f, 0.0f));
		attribs.emplace_back(glm::vec3(origin.x - (size.x * 0.5f), origin.y + aspect * (size.y * 0.5f), 0.0f), glm::vec2(0.0f, 1.0f));
		attribs.emplace_back(glm::vec3(origin.x + (size.x * 0.5f), origin.y - aspect * (size.y * 0.5f), 0.0f), glm::vec2(1.0f, 0.0f));
		attribs.emplace_back(glm::vec3(origin.x + (size.x * 0.5f), origin.y + aspect * (size.y * 0.5f), 0.0f), glm::vec2(1.0f, 1.0f));
	}
	else if (origin_mode == Bottom) {
		attribs.emplace_back(glm::vec3(origin.x - (size.x * 0.5f), origin.y, 0.0f), glm::vec2(0.0f, 0.0f));
		attribs.emplace_back(glm::vec3(origin.x - (size.x * 0.5f), origin.y + aspect * size.y, 0.0f), glm::vec2(0.0f, 1.0f));
		attribs.emplace_back(glm::vec3(origin.x + (size.x * 0.5f), origin.y, 0.0f), glm::vec2(1.0f, 0.0f));
		attribs.emplace_back(glm::vec3(origin.x + (size.x * 0.5f), origin.y + aspect * size.y, 0.0f), glm::vec2(1.0f, 1.0f));
	}
	else if (origin_mode == TopRight) {
		attribs.emplace_back(glm::vec3(origin.x - size.x, origin.y - aspect * size.y, 0.0f), glm::vec2(0.0f, 0.0f));
		attribs.emplace_back(glm::vec3(origin.x - size.x, origin.y, 0.0f), glm::vec2(0.0f, 1.0f));
		attribs.emplace_back(glm::vec3(origin.x, origin.y - aspect * size.y, 0.0f), glm::vec2(1.0f, 0.0f));
		attribs.emplace_back(glm::vec3(origin.x, origin.y, 0.0f), glm::vec2(1.0f, 1.0f));
	}
	
	glBindBuffer(GL_ARRAY_BUFFER, buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(Vert) * attribs.size(), attribs.data(), GL_STREAM_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	
	glUseProgram(program->program);
	glUniformMatrix4fv(program->OBJECT_TO_CLIP_mat4, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));

	glBindTexture(GL_TEXTURE_2D, tex);


	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glBindVertexArray(vao);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, (GLsizei)attribs.size());

	glBindVertexArray(0);


	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);

	glBindTexture(GL_TEXTURE_2D, 0);

	glUseProgram(0);

	GL_ERRORS();
}

void Scene::load(std::string const &filename,
	std::function< void(Scene &, Transform *, std::string const &) > const &on_drawable,
	std::function< void(Scene &, Transform *, std::string const &, std::string const &, std:: string const &, std::string const &) > const &on_portal, 
	std::function< void(Scene &, Transform *, std::string const &) > const &on_button) {

	std::ifstream file(filename, std::ios::binary);

	std::vector< char > names;
	read_chunk(file, "str0", &names);

	struct HierarchyEntry {
		uint32_t parent;
		uint32_t name_begin;
		uint32_t name_end;
		glm::vec3 position;
		glm::quat rotation;
		glm::vec3 scale;
	};
	static_assert(sizeof(HierarchyEntry) == 4 + 4 + 4 + 4*3 + 4*4 + 4*3, "HierarchyEntry is packed.");
	std::vector< HierarchyEntry > hierarchy;
	read_chunk(file, "xfh0", &hierarchy);

	struct PortalEntry {
		uint32_t transform;
		uint32_t name_begin;
		uint32_t name_end;
		uint32_t dest_begin;
		uint32_t dest_end;
		uint32_t walk_mesh_begin;
		uint32_t walk_mesh_end;
		uint32_t group_begin;
		uint32_t group_end;
	};
	static_assert(sizeof(PortalEntry) == 4 + 4 + 4 + 4 + 4 + 4 + 4 + 4 + 4, "PortalEntry is packed.");
	std::vector< PortalEntry > portal_meshes;
	read_chunk(file, "prt0", &portal_meshes);

	struct ButtonEntry {
		uint32_t transform;
		uint32_t name_begin;
		uint32_t name_end;
	};
	static_assert(sizeof(ButtonEntry) == 4 + 4 + 4, "ButtonEntry is packed.");
	std::vector< ButtonEntry > button_meshes;
	read_chunk(file, "btn0", &button_meshes);

	struct MeshEntry {
		uint32_t transform;
		uint32_t name_begin;
		uint32_t name_end;
	};
	static_assert(sizeof(MeshEntry) == 4 + 4 + 4, "MeshEntry is packed.");
	std::vector< MeshEntry > meshes;
	read_chunk(file, "msh0", &meshes);

	struct CameraEntry {
		uint32_t transform;
		char type[4]; //"pers" or "orth"
		float data; //fov in degrees for 'pers', scale for 'orth'
		float clip_near, clip_far;
	};
	static_assert(sizeof(CameraEntry) == 4 + 4 + 4 + 4 + 4, "CameraEntry is packed.");
	std::vector< CameraEntry > loaded_cameras;
	read_chunk(file, "cam0", &loaded_cameras);

	struct LightEntry {
		uint32_t transform;
		char type;
		glm::u8vec3 color;
		float energy;
		float distance;
		float fov;
	};
	static_assert(sizeof(LightEntry) == 4 + 1 + 3 + 4 + 4 + 4, "LightEntry is packed.");
	std::vector< LightEntry > loaded_lights;
	read_chunk(file, "lmp0", &loaded_lights);


	//--------------------------------
	//Now that file is loaded, create transforms for hierarchy entries:

	std::vector< Transform * > hierarchy_transforms;
	hierarchy_transforms.reserve(hierarchy.size());

	for (auto const &h : hierarchy) {
		transforms.emplace_back();
		Transform *t = &transforms.back();
		if (h.parent != -1U) {
			if (h.parent >= hierarchy_transforms.size()) {
				throw std::runtime_error("scene file '" + filename + "' did not contain transforms in topological-sort order.");
			}
			t->parent = hierarchy_transforms[h.parent];
		}

		if (h.name_begin <= h.name_end && h.name_end <= names.size()) {
			t->name = std::string(names.begin() + h.name_begin, names.begin() + h.name_end);
		} else {
				throw std::runtime_error("scene file '" + filename + "' contains hierarchy entry with invalid name indices");
		}

		t->position = h.position;
		t->rotation = h.rotation;
		t->scale = h.scale;

		hierarchy_transforms.emplace_back(t);
	}
	assert(hierarchy_transforms.size() == hierarchy.size());

	for (auto const &p : portal_meshes) {
		std::string dest = "";
		std::string walk_mesh = "";
		std::string group = "";
		if (p.transform >= hierarchy_transforms.size()) {
			throw std::runtime_error("scene file '" + filename + "' contains portal entry with invalid transform index (" + std::to_string(p.transform) + ")");
		}
		if (!(p.name_begin <= p.name_end && p.name_end <= names.size())) {
			throw std::runtime_error("scene file '" + filename + "' contains portal entry with invalid name indices");
		}
		std::string name = std::string(names.begin() + p.name_begin, names.begin() + p.name_end);
		if (p.dest_begin <= p.dest_end && p.dest_end <= names.size()) {
			dest = std::string(names.begin() + p.dest_begin, names.begin() + p.dest_end);
		}
		else {
			throw std::runtime_error("scene file '" + filename + "' contains portal entry with invalid dest name indices (" + std::to_string(p.transform) + ")");
		}
		if (p.walk_mesh_begin <= p.walk_mesh_end && p.walk_mesh_end <= names.size()) {
			walk_mesh = std::string(names.begin() + p.walk_mesh_begin, names.begin() + p.walk_mesh_end);
		}
		else {
			throw std::runtime_error("scene file '" + filename + "' contains portal entry with invalid walk mesh name indices (" + std::to_string(p.transform) + ")");
		}
		if (p.group_begin <= p.group_end && p.group_end <= names.size()) {
			group = std::string(names.begin() + p.group_begin, names.begin() + p.group_end);
		}
		else {
			throw std::runtime_error("scene file '" + filename + "' contains portal entry with invalid group name indices (" + std::to_string(p.transform) + ")");
		}

		if (on_portal) {
			on_portal(*this, hierarchy_transforms[p.transform], name, dest, walk_mesh, group);
		}
	}

	for (auto const &m : meshes) {
		if (m.transform >= hierarchy_transforms.size()) {
			throw std::runtime_error("scene file '" + filename + "' contains mesh entry with invalid transform index (" + std::to_string(m.transform) + ")");
		}
		if (!(m.name_begin <= m.name_end && m.name_end <= names.size())) {
			throw std::runtime_error("scene file '" + filename + "' contains mesh entry with invalid name indices");
		}
		std::string name = std::string(names.begin() + m.name_begin, names.begin() + m.name_end);

		if (on_drawable) {
			on_drawable(*this, hierarchy_transforms[m.transform], name);
		}

	}

	for (auto const &b : button_meshes) {
		if (b.transform >= hierarchy_transforms.size()) {
			throw std::runtime_error("scene file '" + filename + "' contains button entry with invalid transform index (" + std::to_string(b.transform) + ")");
		}
		if (!(b.name_begin <= b.name_end && b.name_end <= names.size())) {
			throw std::runtime_error("scene file '" + filename + "' contains button entry with invalid name indices");
		}
		std::string name = std::string(names.begin() + b.name_begin, names.begin() + b.name_end);

		if (on_button) {
			on_button(*this, hierarchy_transforms[b.transform], name);
		}

	}

	for (auto const &c : loaded_cameras) {
		if (c.transform >= hierarchy_transforms.size()) {
			throw std::runtime_error("scene file '" + filename + "' contains camera entry with invalid transform index (" + std::to_string(c.transform) + ")");
		}
		if (std::string(c.type, 4) != "pers") {
			std::cout << "Ignoring non-perspective camera (" + std::string(c.type, 4) + ") stored in file." << std::endl;
			continue;
		}
		cameras.emplace_back(hierarchy_transforms[c.transform]);
		Camera *camera = &cameras.back();
		camera->fovy = c.data / 180.0f * 3.1415926f; //FOV is stored in degrees; convert to radians.
		camera->near = c.clip_near;
		//N.b. far plane is ignored because cameras use infinite perspective matrices.
	}

	for (auto const &l : loaded_lights) {
		if (l.transform >= hierarchy_transforms.size()) {
			throw std::runtime_error("scene file '" + filename + "' contains lamp entry with invalid transform index (" + std::to_string(l.transform) + ")");
		}
		if (l.type == 'p') {
			//good
		} else if (l.type == 'h') {
			//fine
		} else if (l.type == 's') {
			//okay
		} else if (l.type == 'd') {
			//sure
		} else {
			std::cout << "Ignoring unrecognized lamp type (" + std::string(&l.type, 1) + ") stored in file." << std::endl;
			continue;
		}
		lights.emplace_back(hierarchy_transforms[l.transform]);
		Light *light = &lights.back();
		light->type = static_cast<Light::Type>(l.type);
		light->energy = glm::vec3(l.color) / 255.0f * l.energy;
		light->spot_fov = l.fov / 180.0f * 3.1415926f; //FOV is stored in degrees; convert to radians.
	}

	//load any extra that a subclass wants:
	load_extra(file, names, hierarchy_transforms);

	if (file.peek() != EOF) {
		std::cerr << "WARNING: trailing data in scene file '" << filename << "'" << std::endl;
	}



}

//-------------------------

Scene::Scene(std::string const &filename, std::function< void(Scene &, Transform *, std::string const &) > const &on_drawable,
	std::function< void(Scene &, Transform *, std::string const &, std::string const &, std::string const &, std::string const &) > const &on_portal, 
	std::function< void(Scene &, Transform *, std::string const &) > const &on_button) {
	load(filename, on_drawable, on_portal, on_button);
}

Scene::Scene(Scene const &other) {
	set(other);
}

Scene &Scene::operator=(Scene const &other) {
	set(other);
	return *this;
}

void Scene::set(Scene const &other, std::unordered_map< Transform const *, Transform * > *transform_map_) {

	std::unordered_map< Transform const *, Transform * > t2t_temp;
	std::unordered_map< Transform const *, Transform * > &transform_to_transform = *(transform_map_ ? transform_map_ : &t2t_temp);

	transform_to_transform.clear();

	//null transform maps to itself:
	transform_to_transform.insert(std::make_pair(nullptr, nullptr));

	//Copy transforms and store mapping:
	transforms.clear();
	for (auto const &t : other.transforms) {
		transforms.emplace_back();
		transforms.back().name = t.name;
		transforms.back().position = t.position;
		transforms.back().rotation = t.rotation;
		transforms.back().scale = t.scale;
		transforms.back().parent = t.parent; //will update later

		//store mapping between transforms old and new:
		auto ret = transform_to_transform.insert(std::make_pair(&t, &transforms.back()));
		assert(ret.second);
	}

	//update transform parents:
	for (auto &t : transforms) {
		t.parent = transform_to_transform.at(t.parent);
	}

	//copy other's drawables, updating transform pointers:
	drawables = std::list<Scene::Drawable>();
	for (auto &d : other.drawables) {
		drawables.emplace_back(transform_to_transform.at(d.transform));
		drawables.back().pipeline = d.pipeline;
	}

	//copy other's cameras, updating transform pointers:
	cameras = other.cameras;
	for (auto &c : cameras) {
		c.transform = transform_to_transform.at(c.transform);
	}

	//copy other's lights, updating transform pointers:
	lights = other.lights;
	for (auto &l : lights) {
		l.transform = transform_to_transform.at(l.transform);
	}

	//copy other's portals, updating drawable pointers:
	portals = other.portals;
	for (auto &p : portals) {
		p.second->drawable->transform = transform_to_transform.at(p.second->drawable->transform);
		portal_groups[p.second->group].emplace_back(p.second);
	}

	//copy other's buttons
	buttons = other.buttons;
	for (auto &b : buttons) {
		b.drawable->transform = transform_to_transform.at(b.drawable->transform);
	}
}
