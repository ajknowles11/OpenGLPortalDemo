#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

#include <random>

GLuint basic_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > basic_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("sixrooms.pnct"));
	basic_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > basic_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("sixrooms.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = basic_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = basic_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

WalkMesh const *walkmesh = nullptr;
// Load< WalkMeshes > phonebank_walkmeshes(LoadTagDefault, []() -> WalkMeshes const * {
// 	WalkMeshes *ret = new WalkMeshes(data_path("phone-bank.w"));
// 	walkmesh = &ret->lookup("WalkMesh");
// 	return ret;
// });

PlayMode::PlayMode() : scene(*basic_scene) {
	//create a player transform:
	scene.transforms.emplace_back();
	player.transform = &scene.transforms.back();
	player.transform->position = glm::vec3(3.8, -10.0f, -1.8f);

	//create a player camera attached to a child of the player transform:
	scene.transforms.emplace_back();
	scene.cameras.emplace_back(&scene.transforms.back());
	player.camera = &scene.cameras.back();
	player.camera->fovy = glm::radians(60.0f);
	player.camera->near = 0.01f;
	player.camera->transform->parent = player.transform;

	//player's eyes are 1.8 units above the ground:
	player.camera->transform->position = glm::vec3(0.0f, 0.0f, 1.8f);

	//rotate camera facing direction (-z) to player facing direction (+y):
	player.camera->transform->rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

	//start player walking at nearest walk point:
	if (walkmesh != nullptr) {
		player.at = walkmesh->nearest_walk_point(player.transform->position);
	}

	//set up portals
	for (auto &drawable : scene.drawables) {
		if (drawable.transform->name == "Portal0") {
			auto mesh = basic_meshes->lookup("Portal0");
			portals.emplace_back(new Portal(&drawable, Scene::BoxCollider(mesh.min, mesh.max)));
		}
		else if (drawable.transform->name == "Portal1") {
			auto mesh = basic_meshes->lookup("Portal1");
			portals.emplace_back(new Portal(&drawable, Scene::BoxCollider(mesh.min, mesh.max), portals[0]));
		}
		else if (drawable.transform->name == "Portal2") {
			auto mesh = basic_meshes->lookup("Portal2");
			portals.emplace_back(new Portal(&drawable, Scene::BoxCollider(mesh.min, mesh.max)));
		}
		else if (drawable.transform->name == "Portal3") {
			auto mesh = basic_meshes->lookup("Portal3");
			portals.emplace_back(new Portal(&drawable, Scene::BoxCollider(mesh.min, mesh.max), portals[2]));
		}
		else if (drawable.transform->name == "Portal4") {
			auto mesh = basic_meshes->lookup("Portal4");
			portals.emplace_back(new Portal(&drawable, Scene::BoxCollider(mesh.min, mesh.max), portals[4]));
		}
		else if (drawable.transform->name == "Portal5") {
			auto mesh = basic_meshes->lookup("Portal5");
			portals.emplace_back(new Portal(&drawable, Scene::BoxCollider(mesh.min, mesh.max)));
		}
		else if (drawable.transform->name == "Sphere") {
			debug_sphere = drawable.transform;
		}
	}

}

PlayMode::~PlayMode() {
	for (auto p : portals) {
		if (p != nullptr) {
			delete p;
		}
	}
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_ESCAPE) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			return true;
		} else if (evt.key.keysym.sym == SDLK_a) {
			left.downs += 1;
			left.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.downs += 1;
			right.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.downs += 1;
			up.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.downs += 1;
			down.pressed = true;
			return true;
		}
	} else if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_a) {
			left.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.pressed = false;
			return true;
		}
	} else if (evt.type == SDL_MOUSEBUTTONDOWN) {
		if (SDL_GetRelativeMouseMode() == SDL_FALSE) {
			SDL_SetRelativeMouseMode(SDL_TRUE);
			return true;
		}
	} else if (evt.type == SDL_MOUSEMOTION) {
		if (SDL_GetRelativeMouseMode() == SDL_TRUE) {
			glm::vec2 motion = glm::vec2(
				evt.motion.xrel / float(window_size.y),
				-evt.motion.yrel / float(window_size.y)
			);
			glm::vec3 upDir = player.uses_walkmesh ? walkmesh->to_world_smooth_normal(player.at) : player.transform->make_local_to_world() * glm::vec4(0,0,1,0);
			player.transform->rotation = glm::angleAxis(-motion.x * player.camera->fovy, upDir) * player.transform->rotation;

			float pitch = glm::pitch(player.camera->transform->rotation);
			pitch += motion.y * player.camera->fovy;
			//camera looks down -z (basically at the player's feet) when pitch is at zero.
			pitch = std::min(pitch, 0.95f * 3.1415926f);
			pitch = std::max(pitch, 0.05f * 3.1415926f);
			player.camera->transform->rotation = glm::angleAxis(pitch, glm::vec3(1.0f, 0.0f, 0.0f));

			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {
	//player walking:

	//combine inputs into a move:
		constexpr float PlayerSpeed = 5.0f;
		glm::vec2 move = glm::vec2(0.0f);
		if (left.pressed && !right.pressed) move.x =-1.0f;
		if (!left.pressed && right.pressed) move.x = 1.0f;
		if (down.pressed && !up.pressed) move.y =-1.0f;
		if (!down.pressed && up.pressed) move.y = 1.0f;

		//make it so that moving diagonally doesn't go faster:
		if (move != glm::vec2(0.0f)) move = glm::normalize(move) * PlayerSpeed * elapsed;

		//get move in world coordinate system:
		glm::vec3 remain = player.transform->make_local_to_world() * glm::vec4(move.x, move.y, 0.0f, 0.0f);

	if (player.uses_walkmesh) {
		//using a for() instead of a while() here so that if walkpoint gets stuck in
		// some awkward case, code will not infinite loop:
		for (uint32_t iter = 0; iter < 10; ++iter) {
			if (remain == glm::vec3(0.0f)) break;
			WalkPoint end;
			float time;
			walkmesh->walk_in_triangle(player.at, remain, &end, &time);
			player.at = end;
			if (time == 1.0f) {
				//finished within triangle:
				remain = glm::vec3(0.0f);
				break;
			}
			//some step remains:
			remain *= (1.0f - time);
			//try to step over edge:
			glm::quat rotation;
			if (walkmesh->cross_edge(player.at, &end, &rotation)) {
				//stepped to a new triangle:
				player.at = end;
				//rotate step to follow surface:
				remain = rotation * remain;
			} else {
				//ran into a wall, bounce / slide along it:
				glm::vec3 const &a = walkmesh->vertices[player.at.indices.x];
				glm::vec3 const &b = walkmesh->vertices[player.at.indices.y];
				glm::vec3 const &c = walkmesh->vertices[player.at.indices.z];
				glm::vec3 along = glm::normalize(b-a);
				glm::vec3 normal = glm::normalize(glm::cross(b-a, c-a));
				glm::vec3 in = glm::cross(normal, along);

				//check how much 'remain' is pointing out of the triangle:
				float d = glm::dot(remain, in);
				if (d < 0.0f) {
					//bounce off of the wall:
					remain += (-1.25f * d) * in;
				} else {
					//if it's just pointing along the edge, bend slightly away from wall:
					remain += 0.01f * d * in;
				}
			}
		}

		if (remain != glm::vec3(0.0f)) {
			std::cout << "NOTE: code used full iteration budget for walking." << std::endl;
		}

		//update player's position to respect walking:
		player.transform->position = walkmesh->to_world_point(player.at);

		{ //update player's rotation to respect local (smooth) up-vector:
			
			glm::quat adjust = glm::rotation(
				player.transform->rotation * glm::vec3(0.0f, 0.0f, 1.0f), //current up vector
				walkmesh->to_world_smooth_normal(player.at) //smoothed up vector at walk location
			);
			player.transform->rotation = glm::normalize(adjust * player.transform->rotation);
		}

		/*
		glm::mat4x3 frame = camera->transform->make_local_to_parent();
		glm::vec3 right = frame[0];
		//glm::vec3 up = frame[1];
		glm::vec3 forward = -frame[2];

		camera->transform->position += move.x * right + move.y * forward;
		*/
	}
	else {
		player.transform->position += remain;
	}

	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;

	update_physics(elapsed);
}

void PlayMode::update_physics(float elapsed) {
	
	auto point_in_box = [](glm::vec3 x, glm::vec3 min, glm::vec3 max){
		return (min.x <= x.x && x.x <= max.x) && (min.y <= x.y && x.y <= max.y) && (min.z <= x.z && x.z <= max.z);
	};

	for (auto const &p : portals) {
		glm::mat4 p_world = p->drawable->transform->make_local_to_world();
		glm::mat4 player_world = player.transform->make_local_to_world();
		glm::mat4 t_local = p->twin->drawable->transform->make_world_to_local();

		glm::mat4 m = p_world * t_local * player_world;
		p->camera->transform->position = m * glm::vec4(player.camera->transform->position, 1);
		p->camera->transform->rotation = m * glm::mat4(player.camera->transform->rotation);
		p->camera->aspect = player.camera->aspect;
		p->camera->fovy = player.camera->fovy;
		p->camera->near = player.camera->near;

		glm::vec3 offset_from_portal = player.transform->position - glm::vec3(p_world * glm::vec4(0,0,0,1));
		bool now_in_front = 0 <= glm::dot(offset_from_portal, glm::normalize(glm::vec3(p_world[1])));
		if (now_in_front == p->player_in_front) {
			p->sleeping = false;
			continue;
		}
		p->player_in_front = now_in_front;
		if (p->sleeping) {
			p->sleeping = false;
			continue;
		}
		glm::mat4 p_local = p->drawable->transform->make_world_to_local();
		if (point_in_box(p_local * glm::vec4(player.transform->position, 1), p->box.min, p->box.max)) {
			//teleport
			//std::cout << "teleported" << "\n";

			glm::mat4 const m_reverse = glm::mat4(p->twin->drawable->transform->make_local_to_world()) * glm::mat4(p_local);

			player.transform->position = m_reverse * glm::vec4(player.transform->position, 1);
			player.transform->rotation = m_reverse * glm::mat4(player.transform->rotation);
			p->twin->sleeping = true;
		}
	}
}

// https://th0mas.nl/2013/05/19/rendering-recursive-portals-with-opengl/
// https://github.com/ThomasRinsma/opengl-game-test/blob/8363bbf/src/scene.cc
void PlayMode::draw_recursive_portals(Scene::Camera camera, glm::vec4 clip_plane, GLint max_depth, GLint current_depth) {
	for (auto &p : portals) {

		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		glDepthMask(GL_FALSE);

		glDisable(GL_DEPTH_TEST);

		glEnable(GL_STENCIL_TEST);

		glStencilFunc(GL_NOTEQUAL, current_depth, 0xFF);

		glStencilOp(GL_INCR, GL_KEEP, GL_KEEP);

		glStencilMask(0xFF);

		//draw portal in stencil buffer
		scene.draw_one(*p->twin->drawable, camera, true, p->twin->get_self_clip_plane());

		if (current_depth == max_depth) {
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			glDepthMask(GL_TRUE);

			glClear(GL_DEPTH_BUFFER_BIT);

			glEnable(GL_DEPTH_TEST);

			glEnable(GL_STENCIL_TEST);

			glStencilMask(0x00);

			glStencilFunc(GL_EQUAL, current_depth + 1, 0xFF);

			//draw non portals using portal proj matrix ( camera with near plane set to portal (oblique??) )
			scene.draw(*p->camera, true, p->get_clipping_plane());
		}
		else {
			draw_recursive_portals(*p->camera, p->get_clipping_plane(), max_depth, current_depth + 1);
		}

		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		glDepthMask(GL_FALSE);

		glEnable(GL_STENCIL_TEST);
		glStencilMask(0xFF);

		glStencilFunc(GL_NOTEQUAL, current_depth + 1, 0xFF);

		glStencilOp(GL_DECR, GL_KEEP, GL_KEEP);

		//draw portal in stencil buffer
		scene.draw_one(*p->twin->drawable, camera, true, p->twin->get_self_clip_plane());
	}

	glDisable(GL_STENCIL_TEST);
	glStencilMask(0x00);

	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);

	glDepthFunc(GL_ALWAYS);

	glClear(GL_DEPTH_BUFFER_BIT);

	for (auto &p : portals) {
		scene.draw_one(*p->drawable, camera, true, p->get_self_clip_plane());
	}

	glDepthFunc(GL_LESS);

	glEnable(GL_STENCIL_TEST);
	glStencilMask(0x00);

	glStencilFunc(GL_LEQUAL, current_depth, 0xFF);

	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDepthMask(GL_TRUE);

	glEnable(GL_DEPTH_TEST);

	scene.draw(camera, true, clip_plane);

}

void PlayMode::draw(glm::uvec2 const &drawable_size) {

	//update camera aspect ratio for drawable:
	player.camera->aspect = float(drawable_size.x) / float(drawable_size.y);;

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	glClearColor(0.5f, 0.7f, 0.9f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	glm::mat4x3 const player_cam_world = player.camera->transform->make_local_to_world();

	draw_recursive_portals(*player.camera, glm::vec4(-player_cam_world[2],
			 -glm::dot(player_cam_world * glm::vec4(0,0,0,1), -player_cam_world[2])), 0, 0);

	/* In case you are wondering if your walkmesh is lining up with your scene, try:
	{
		glDisable(GL_DEPTH_TEST);
		DrawLines lines(player.camera->make_projection() * glm::mat4(player.camera->transform->make_world_to_local()));
		for (auto const &tri : walkmesh->triangles) {
			lines.draw(walkmesh->vertices[tri.x], walkmesh->vertices[tri.y], glm::u8vec4(0x88, 0x00, 0xff, 0xff));
			lines.draw(walkmesh->vertices[tri.y], walkmesh->vertices[tri.z], glm::u8vec4(0x88, 0x00, 0xff, 0xff));
			lines.draw(walkmesh->vertices[tri.z], walkmesh->vertices[tri.x], glm::u8vec4(0x88, 0x00, 0xff, 0xff));
		}
	}
	*/

	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		constexpr float H = 0.09f;
		lines.draw_text("Mouse motion looks; WASD moves; escape ungrabs mouse",
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text("Mouse motion looks; WASD moves; escape ungrabs mouse",
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
	}
	GL_ERRORS();
}

// gets clipping plane used by portal's camera
glm::vec4 PlayMode::Portal::get_clipping_plane() {
	glm::mat4x3 const p_world = drawable->transform->make_local_to_world();
	glm::vec3 p_forward = glm::normalize(p_world[1]);
	glm::vec3 const p_origin = glm::vec3(p_world * glm::vec4(0,0,0,1));
	glm::vec3 const camera_offset_from_portal = camera->transform->position - p_origin;
	if (glm::dot(p_forward, camera_offset_from_portal) >= 0) p_forward *= -1;

	return glm::vec4(p_forward, -glm::dot(p_origin, p_forward));
}

// gets clipping plane used for portal mesh itself (stop flicker)
glm::vec4 PlayMode::Portal::get_self_clip_plane() {
	glm::mat4x3 const p_world = drawable->transform->make_local_to_world();
	glm::mat4x3 const t_world = twin->drawable->transform->make_local_to_world();
	glm::vec3 p_forward = glm::normalize(p_world[1]);
	glm::vec3 t_forward = glm::normalize(t_world[1]);
	glm::vec3 const p_origin = glm::vec3(p_world * glm::vec4(0,0,0,1));
	glm::vec3 const t_origin = glm::vec3(t_world * glm::vec4(0,0,0,1));
	glm::vec3 const camera_offset_from_portal = twin->camera->transform->position - t_origin;
	if (glm::dot(t_forward, camera_offset_from_portal) >= 0) p_forward *= -1;

	return glm::vec4(p_forward, -glm::dot(p_origin, p_forward));
}
