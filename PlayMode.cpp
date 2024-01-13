#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"
#include "ColorTextureProgram.hpp"
#include "SolidOutlineProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

#include <random>

GLuint meshes_for_lit_color_texture_program = 0;
GLuint meshes_for_color_texture_program = 0;
Load< MeshBuffer > level_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("level/demo.pnct"));
	meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	meshes_for_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > level_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("level/demo.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = level_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;
		drawable.pipeline.vao = meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	}, [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name, std::string const &dest_name, std::string const &walk_mesh_name, std::string const &group_name){
		Mesh const &mesh = level_meshes->lookup(mesh_name);

		Scene::Drawable *drawable = new Scene::Drawable(transform);

		drawable->pipeline = lit_color_texture_program_pipeline;
		drawable->pipeline.vao = meshes_for_lit_color_texture_program;
		drawable->pipeline.type = mesh.type;
		drawable->pipeline.start = mesh.start;
		drawable->pipeline.count = mesh.count;

		//why transform name and not mesh name? well the portal data addon in blender uses transform to point to destination.
		//so when we link portals up here, we need to make sure we can index using the dest transform's name (not mesh name).
		//We should probably make these the same anyway just because it's nicer.
		Scene::Portal *&portal = scene.portals[transform->name];
		if (dest_name != "") {
			Scene::Portal *&dest = scene.portals[dest_name];

			//dest portal may have already been created in the scene. if not we make a temp one now (keep its pointer value the same later)
			if (dest == nullptr) { //not already created
				dest = new Scene::Portal();
			}
			//now we can assign it when we create this portal

			//this portal may already have been created as a dest portal for an earlier one, in which case we need to keep the pointer the same.
			if (portal == nullptr) { //not already created
				portal = new Scene::Portal(drawable, Scene::BoxCollider(mesh.min, mesh.max), walk_mesh_name, dest, group_name);
			}
			else { //already created (don't change pointer, just value pointed to)
				*portal = Scene::Portal(drawable, Scene::BoxCollider(mesh.min, mesh.max), walk_mesh_name, dest, group_name);
			}
		}
		else {
			//this portal may already have been created as a dest portal for an earlier one, in which case we need to keep the pointer the same.
			if (portal == nullptr) { //not already created
				portal = new Scene::Portal(drawable, Scene::BoxCollider(mesh.min, mesh.max), walk_mesh_name, group_name);
			}
			else { //already created (don't change pointer, just value pointed to)
				*portal = Scene::Portal(drawable, Scene::BoxCollider(mesh.min, mesh.max), walk_mesh_name, group_name);
			}
		}

		scene.portal_groups[group_name].emplace_back(portal);

	}, [&](Scene &scene, Scene::Transform *transform, std::string const &button_name){
		Mesh const &mesh = level_meshes->lookup(button_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;
		drawable.pipeline.vao = meshes_for_lit_color_texture_program;
		
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

		scene.buttons.emplace_back(&drawable, mesh.min, mesh.max, button_name);
	});
});

Load< WalkMeshes > walkmeshes(LoadTagDefault, []() -> WalkMeshes const * {
	WalkMeshes *ret = new WalkMeshes(data_path("level/demo.w"));
	return ret;
});

Load< Scene::FullTriProgram > full_tri_program(LoadTagEarly, []() -> Scene::FullTriProgram const * {
	return new Scene::FullTriProgram();
});

// textures ------------------

Load< Scene::Texture > cursor_texture(LoadTagDefault, []() -> Scene::Texture const * {
	return new Scene::Texture(data_path("textures/cursor.png"));
});

Load< Scene::Texture > mouse_texture(LoadTagDefault, []() -> Scene::Texture const * {
	return new Scene::Texture(data_path("textures/mouse.png"));
});

Load< Scene::Texture > controls_texture(LoadTagDefault, []() -> Scene::Texture const * {
	return new Scene::Texture(data_path("textures/controls.png"));
});

Load< Scene::Texture > pause_texture(LoadTagDefault, []() -> Scene::Texture const * {
	return new Scene::Texture(data_path("textures/pause.png"));
});

Load< Scene::Texture > dingus_texture(LoadTagDefault, []() -> Scene::Texture const * {
	return new Scene::Texture(data_path("textures/dingus_nowhiskers.png"));
});

Load< Scene::Texture > wood_texture(LoadTagDefault, []() -> Scene::Texture const * {
	return new Scene::Texture(data_path("textures/wood.png"));
});

Load< Scene::Texture > brick_texture(LoadTagDefault, []() -> Scene::Texture const * {
	return new Scene::Texture(data_path("textures/stonebrick.png"));
});


// ---------------------------

PlayMode::PlayMode() : scene(*level_scene) {
	scene.full_tri_program = *full_tri_program;

	//create a player transform:
	scene.transforms.emplace_back();
	player.transform = &scene.transforms.back();
	player.transform->position = glm::vec3(0.0f, 0.0f, 0.0f);
	player.transform->rotation = glm::quat(glm::vec3(0.0f, 0.0f, glm::radians(-90.0f)));

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
	player.camera->transform->rotation = glm::quat(glm::vec3(glm::radians(80.0f), 0.0f, 0.0f));

	//gather walkmeshes
	{
		for (auto const &p : scene.portals) {
			if (walkmesh_map.count(p.second->on_walkmesh) == 0) {
				walkmesh_map.emplace(p.second->on_walkmesh, &walkmeshes->lookup(p.second->on_walkmesh));
			}
		}
	}

	walkmesh = walkmesh_map["wm0"];

	//start player walking at nearest walk point:
	if (walkmesh != nullptr) {
		player.at = walkmesh->nearest_walk_point(player.transform->position);
	}

	scene.current_group = &scene.portal_groups["Start"];

	for (auto &t : scene.transforms) {
		if (t.name == "RotateBase") {
			rotate_base = &t;
		}
	}
	
	//Button scripting
	for (auto &b : scene.buttons) {
		// Example of how a button named "Lever" might be set up to rotate a transform.
		if (b.name == "Lever") {
			b.on_pressed = [&](){
				b.hit = !b.hit;
				b.active = false;

				// start or stop portal spinning

				//we could also init timer once (without auto start/delete) and just save a ref, but it's not a big deal
				timers.emplace_back(0.12f, [&](float alpha){
					float angle = b.hit ? glm::mix(0.0f, 90.0f, alpha) : glm::mix(90.0f, 0.0f, alpha);
					b.drawable->transform->rotation = glm::angleAxis(glm::radians(angle), glm::vec3(1,0,0));
				}, [&](){
					b.active = true;
				});
			};
		}
	}

	//Texture hookup
	{
		auto make_tex = [](Load<Scene::Texture> texture, int wrap_s, int wrap_t, int mag_filt, int min_filt){
			GLuint tex = 0;

			glGenTextures(1, &tex);

			glBindTexture(GL_TEXTURE_2D, tex);
		
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture->size.x, texture->size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, texture->pixels.data());

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_s);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_t);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filt);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filt);
			glBindTexture(GL_TEXTURE_2D, 0);

			return tex;
		};

		GLuint dingus_tex = make_tex(dingus_texture, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, GL_LINEAR, GL_LINEAR);
		GLuint wood_tex = make_tex(wood_texture, GL_REPEAT, GL_REPEAT, GL_LINEAR, GL_LINEAR);
		GLuint brick_tex = make_tex(brick_texture, GL_REPEAT, GL_REPEAT, GL_LINEAR, GL_LINEAR);
		

		for (auto &d : scene.drawables) {
			if (d.transform->name == "dingus") {
				d.pipeline.textures->texture = dingus_tex;
			}
			if (d.transform->name.substr(0, 5) == "Floor" || d.transform->name.substr(0, 4) == "Ceil") {
				d.pipeline.textures->texture = wood_tex;
			}
			if (d.transform->name.substr(0, 4) == "Wall") {
				d.pipeline.textures->texture = brick_tex;
			}
		}
	}


	//ScreenImage UI elements
	cursor = Scene::ScreenImage(*cursor_texture, glm::vec2(0), glm::vec2(0.0125f), Scene::ScreenImage::Center, color_texture_program);
	mouse_prompt = Scene::ScreenImage(*mouse_texture, glm::vec2(0), glm::vec2(0.04f), Scene::ScreenImage::Center, color_texture_program);

	constexpr float cont_hint_width = 0.2f * 16.0f / 9.0f;
	controls_hint = Scene::ScreenImage(*controls_texture, glm::vec2(0.9f, 0.9f), glm::vec2(cont_hint_width, 0.2f), Scene::ScreenImage::TopRight, color_texture_program);
	pause_text = Scene::ScreenImage(*pause_texture, glm::vec2(0), glm::vec2(0.4f), Scene::ScreenImage::Center, color_texture_program);

}

PlayMode::~PlayMode() {
	for (auto p : scene.portals) {
		if (p.second != nullptr) {
			delete p.second;
		}
	}
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_ESCAPE) {
			if (!paused) {
				GLfloat viewportarr[4];
				glGetFloatv(GL_VIEWPORT, viewportarr);
				SDL_WarpMouseInWindow(SDL_GL_GetCurrentWindow(), (int)viewportarr[2] / 2, (int)viewportarr[3] / 2);
				SDL_SetRelativeMouseMode(SDL_FALSE);
			}
			else {
				SDL_SetRelativeMouseMode(SDL_TRUE);
			}
			paused = !paused;
			return true;
		} if (paused) {
			return false;
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
		} else if (evt.key.keysym.sym == SDLK_LSHIFT) {
			shift.downs += 1;
			shift.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_F1) {
			hide_overlay.downs += 1;
			hide_overlay.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_UP) {
			up_arrow.downs += 1;
			up_arrow.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_DOWN) {
			down_arrow.downs += 1;
			down_arrow.pressed = true;
			return true;
		}
	} else if (evt.type == SDL_KEYUP) {
		if (paused) return false;
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
		} else if (evt.key.keysym.sym == SDLK_LSHIFT) {
			shift.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_F1) {
			hide_overlay.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_UP) {
			up_arrow.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_DOWN) {
			down_arrow.pressed = false;
			return true;
		}
	} else if (evt.type == SDL_MOUSEBUTTONDOWN) {
		if (SDL_GetRelativeMouseMode() == SDL_FALSE) {
			SDL_SetRelativeMouseMode(SDL_TRUE);
			// unpause if paused
			if (paused) {
				paused = false;
			}
			return true;
		} else if (paused) {
			return false;
		}
		else {
			if (evt.button.button == SDL_BUTTON_LEFT) {
				click.downs += 1;
				click.pressed = true;
				return true;
			}
		}
	} else if (paused) {
		return false;
	} else if (evt.type == SDL_MOUSEBUTTONUP) {
		if (evt.button.button == SDL_BUTTON_LEFT) {
			click.pressed = false;
			return true;
		}
	} else if (evt.type == SDL_MOUSEMOTION) {
		if (SDL_GetRelativeMouseMode() == SDL_TRUE) {
			glm::vec2 motion = glm::vec2(
				evt.motion.xrel / float(window_size.y),
				-evt.motion.yrel / float(window_size.y)
			);
			glm::vec3 upDir = player.transform->make_local_to_world() * glm::vec4(0,0,1,0);
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
	if (paused) return;

	//interaction with buttons
	constexpr float MaxButtonPollRange2 = 20.0f; //speed up checking a bit by ignoring far buttons
	{
		glm::mat4 const &cam_to_world = player.camera->transform->make_local_to_world();
		glm::vec4 const &cam_invdir = glm::vec4(glm::vec3(1.0f / -cam_to_world[2]), 0);
		glm::vec4 const &cam_origin = cam_to_world[3];

		player.show_mouse_prompt = false;
		for (auto &b : scene.buttons) {
			if (!b.active) continue;
			if (glm::distance2(glm::vec3(cam_origin), b.drawable->transform->make_local_to_world()[3]) > MaxButtonPollRange2) continue;
			glm::mat4x3 const &b_to_local = b.drawable->transform->make_world_to_local();
			if (Scene::ray_bbox_hit(b.box, b_to_local * cam_invdir, b_to_local * cam_origin, 1.7f * b.range_mult)) {
				player.show_mouse_prompt = true;
				if (click.pressed && !click.last_pressed && b.on_pressed) b.on_pressed();
				break;
			}
		}
	}


	//player walking:
	{
		//combine inputs into a move:
		constexpr float PlayerSpeed = 5.0f;
		constexpr float PlayerSprintSpeed = 8.0f;
		glm::vec2 move = glm::vec2(0.0f);
		if (left.pressed && !right.pressed) move.x =-1.0f;
		if (!left.pressed && right.pressed) move.x = 1.0f;
		if (down.pressed && !up.pressed) move.y =-1.0f;
		if (!down.pressed && up.pressed) move.y = 1.0f;

		//make it so that moving diagonally doesn't go faster:
		if (move != glm::vec2(0.0f)) move = shift.pressed ? 
			glm::normalize(move) * PlayerSprintSpeed * elapsed : glm::normalize(move) * PlayerSpeed * elapsed;
		

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

			// This stuff is nice for walking on walls and such as you can automatically rotate with the surface normal. 
			// But in normal use it makes inclines weird (you tilt when walking up stairs). I just kept it here for possible future puzzles.

			// { //update player's rotation to respect local (smooth) up-vector:
				
			// 	glm::quat adjust = glm::rotation(
			// 		player.transform->rotation * glm::vec3(0.0f, 0.0f, 1.0f), //current up vector
			// 		walkmesh->to_world_smooth_normal(player.at) //smoothed up vector at walk location
			// 	);
			// 	player.transform->rotation = glm::normalize(adjust * player.transform->rotation);
			// }

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
	}

	//debug stuff
	frame_delta = elapsed;
	if (hide_overlay.pressed && !hide_overlay.last_pressed) {
		hide_all_overlays = !hide_all_overlays;
	}
	if (up_arrow.pressed && !up_arrow.last_pressed) {
		scene.default_draw_recursion_max += 1;
	}
	if (down_arrow.pressed && !down_arrow.last_pressed && scene.default_draw_recursion_max > 0) {
		scene.default_draw_recursion_max -= 1;
	}

	//button cleanup
	{
		//reset button press counters:
		left.downs = 0;
		right.downs = 0;
		up.downs = 0;
		down.downs = 0;
		shift.downs = 0;
		click.downs = 0;
		hide_overlay.downs = 0;
		up_arrow.downs = 0;
		down_arrow.downs = 0;

		//and adjust last_pressed:
		left.last_pressed = left.pressed;
		right.last_pressed = right.pressed;
		up.last_pressed = up.pressed;
		down.last_pressed = down.pressed;
		shift.last_pressed = shift.pressed;
		click.last_pressed = click.pressed;
		hide_overlay.last_pressed = hide_overlay.pressed;
		up_arrow.last_pressed = up_arrow.pressed;
		down_arrow.last_pressed = down_arrow.pressed;
	}

	handle_portals();

	{ //update listener to camera position:
		glm::mat4x3 frame = player.camera->transform->make_local_to_world();
		glm::vec3 frame_right = frame[0];
		glm::vec3 frame_at = frame[3];
		Sound::listener.set_position_right(frame_at, frame_right, 1.0f / 60.0f);
	}

	for (auto &t : timers) {
		if (t.active) {
			t.tick(elapsed);
		}
	}
	for (size_t i = timers.size(); i > 0; i--) {
		if (timers[i-1].queued_for_delete) {
			timers.erase(timers.begin() + i - 1);
		}
	}

}

void PlayMode::handle_portals() {

	for (auto const &pair : scene.portals) {
		Scene::Portal *p = pair.second;
		if (p->dest == nullptr) continue;
		if (!p->active) continue;
		
		glm::mat4 world_to_portal = p->drawable->transform->make_world_to_local();
		glm::vec3 offset_from_portal = world_to_portal * glm::vec4(player.transform->position, 1);

		// only consider player within tracking box
		if (!Scene::point_in_box(offset_from_portal, p->tracking_box.min, p->tracking_box.max)) {
			p->player_tracked = false;
			continue;
		}

		// if just entered tracking box don't tp (could have stepped out of box on side A, stepped in on side B which would pass all further checks and cause improper tp)
		if (!p->player_tracked) {
			p->player_tracked = true;
			p->player_last_pos = offset_from_portal;
			continue;
		}
		
		bool now_in_front = 0 < glm::dot(offset_from_portal, glm::vec3(0,1,0));
		// confirm player crossed portal xz plane
		if (now_in_front == p->player_in_front) {
			p->sleeping = false;
			p->player_last_pos = offset_from_portal;
			continue;
		}
		p->player_in_front = now_in_front;

		// don't tp if sleeping, meaning we just came from this portal (could have technically crossed plane)
		if (p->sleeping) {
			p->sleeping = false;
			p->player_last_pos = offset_from_portal;
			continue;
		}

		// lastly, check if player passed through portal
		if (Scene::line_bbox_hit(p->player_last_pos, offset_from_portal, p->tp_box.min, p->tp_box.max)) {
			// Teleport the player (or object)

			glm::mat4x3 const &dest_to_world = p->dest->drawable->transform->make_local_to_world();
			glm::mat4 const &portal_to_dest_mat = glm::mat4(dest_to_world) * glm::mat4(world_to_portal);

			player.transform->position = portal_to_dest_mat * glm::vec4(player.transform->position, 1);
			player.transform->rotation = portal_to_dest_mat * glm::mat4(player.transform->rotation);
			// I considered working with scale here but didn't end up getting it working and moved on from that puzzle idea anyway

			// Stop destination from teleporting for 1 frame (so we don't instantly return)
			p->dest->sleeping = true;

			// Below stuff is more specific to this game/implementation. 

			// We only draw portals in one "active" group at a time, so when we teleport we need to activate whatever group the destination portal is in
			scene.current_group = &scene.portal_groups[p->dest->group];

			// And we use walkmesh for movement so we have to make sure the current walkmesh is the one on which the destination portal sits
			walkmesh = walkmesh_map[p->dest->on_walkmesh];
			if (walkmesh != nullptr) {
				player.at = walkmesh->nearest_walk_point(player.transform->position);
			}
			
		}
		p->player_last_pos = offset_from_portal;
	}
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {

	//update camera aspect ratio for drawable:
	player.camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glClearColor(0.8f, 0.8f, 0.8f, 1.0f);
	glClearDepth(1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT); 

	GL_ERRORS();

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 3);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::normalize(glm::vec3(0.4f, 0.1f, -0.8f))));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.96f)));
	glUseProgram(0);

	scene.draw(*player.camera);

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

	// now draw UI elements
	float aspect = float(drawable_size.x) / float(drawable_size.y);

	if (paused) {
		pause_text.draw(aspect);
	}

	if (!hide_all_overlays) {
		// cursor
		if (player.show_mouse_prompt) {
			mouse_prompt.draw(aspect);
		}
		else {
			cursor.draw(aspect);
		}

		// controls
		controls_hint.draw(aspect);

		{ //use DrawLines to overlay some text:
			glDisable(GL_DEPTH_TEST);
			DrawLines lines(glm::mat4(
				1.0f / aspect, 0.0f, 0.0f, 0.0f,
				0.0f, 1.0f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f,
				0.0f, 0.0f, 0.0f, 1.0f
			));

			constexpr float H = 0.09f;
			float ofs = 2.0f / drawable_size.y;

			std::string const &fps = std::to_string(int(glm::round(1.0f / frame_delta))) + " FPS";
			lines.draw_text(fps,
			glm::vec3(-aspect + 0.1f * H, 0.99f - H, 0.0f),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
			lines.draw_text(fps,
			glm::vec3(-aspect + 0.1f * H + ofs, 0.99f - H + ofs, 0.0f),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
			
			std::string const &recursion_lvl = "Portal recursion count: " + std::to_string(int(scene.default_draw_recursion_max));
			lines.draw_text(recursion_lvl, 
			glm::vec3(-aspect + 0.1f * H, 0.99f - 2.0f * H + ofs, 0.0f),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
			lines.draw_text(recursion_lvl,
			glm::vec3(-aspect + 0.1f * H + ofs, 0.99f - 2.0f * H + 2.0f * ofs, 0.0f),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		}

	}
	GL_ERRORS();
}