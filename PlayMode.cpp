#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "Sound.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

#include <random>

GLuint meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > level_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("level/level.pnct"));
	meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > level_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("level/level.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = level_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	}, [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name, std::string const &dest_name, std::string const &walk_mesh_name){
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
				portal = new Scene::Portal(drawable, Scene::BoxCollider(mesh.min, mesh.max), walk_mesh_name, dest);
			}
			else { //already created (don't change pointer, just value pointed to)
				*portal = Scene::Portal(drawable, Scene::BoxCollider(mesh.min, mesh.max), walk_mesh_name, dest);
			}
		}
		else {
			//this portal may already have been created as a dest portal for an earlier one, in which case we need to keep the pointer the same.
			if (portal == nullptr) { //not already created
				portal = new Scene::Portal(drawable, Scene::BoxCollider(mesh.min, mesh.max), walk_mesh_name);
			}
			else { //already created (don't change pointer, just value pointed to)
				*portal = Scene::Portal(drawable, Scene::BoxCollider(mesh.min, mesh.max), walk_mesh_name);
			}
		}

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
	WalkMeshes *ret = new WalkMeshes(data_path("level/walkmeshes.w"));
	return ret;
});

Load< Scene::FullTriProgram > full_tri_program(LoadTagEarly, []() -> Scene::FullTriProgram const * {
	return new Scene::FullTriProgram();
});

PlayMode::PlayMode() : scene(*level_scene) {
	scene.full_tri_program = *full_tri_program;

	//create a player transform:
	scene.transforms.emplace_back();
	player.transform = &scene.transforms.back();
	//player.transform->position = glm::vec3(3.8, -10.0f, -1.8f);
	player.transform->position = glm::vec3(3.8, -10.0f, 0.0f);

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

	//gather walkmeshes
	{
		for (auto const &p : scene.portals) {
			if (walkmesh_map.count(p.second->on_walkmesh) == 0) {
				walkmesh_map.emplace(p.second->on_walkmesh, &walkmeshes->lookup(p.second->on_walkmesh));
			}
		}
	}

	walkmesh = walkmesh_map["ApartmentWalkMesh"];

	//start player walking at nearest walk point:
	if (walkmesh != nullptr) {
		player.at = walkmesh->nearest_walk_point(player.transform->position);
	}


	

	//Button scripting
	for (auto &b : scene.buttons) {
		if (b.name == "FridgeDoor") {
			b.on_pressed = [&](){
				b.active = false;
				scene.portals["PortalFridge"]->active = true;
				scene.portals["Drop0"]->active = true;
				scene.portals["Drop1"]->active = true;
				scene.portals["StairPortalA"]->active = true;
				scene.portals["StairPortalB"]->active = true;
				scene.portals["StairPortal0"]->active = true;
				scene.portals["StairPortal1"]->active = true;
				scene.portals["StairPortal2"]->active = true;
				scene.portals["StairPortal3"]->active = true;
				scene.portals["StairPortal0b"]->active = true;
				scene.portals["StairPortal1b"]->active = true;
				scene.portals["StairPortal2b"]->active = true;
				scene.portals["StairPortal3b"]->active = true;
				player.animation_lock_move = true;
				player.animation_lock_look = true;
				player.uses_walkmesh = false;
				glm::vec3 const &start_pos = player.transform->position;
				timers.emplace_back(0.8f, [&](float alpha){
						glm::vec3 const &target = glm::vec3(-4.5f, 0.5f, -0.2f);
						player.transform->position = glm::mix(start_pos, target, alpha);
						player.transform->rotation = glm::lerp(player.transform->rotation, glm::angleAxis(glm::radians(180.0f), glm::vec3(0,0,1)), alpha);
						player.camera->transform->rotation = glm::lerp(player.camera->transform->rotation, glm::angleAxis(1.1f * glm::radians(90.0f), glm::vec3(1,0,0)), alpha);
						b.drawable->transform->rotation = glm::lerp(glm::quat(), glm::angleAxis(glm::radians(90.0f), glm::vec3(0,0,1)), alpha);
					}, [&](){
						timers.emplace_back(0.4f, [&](float alpha){
							glm::vec3 const &start = glm::vec3(-4.5f, 0.5f, -0.2f);
							glm::vec3 const &end = glm::vec3(-4.5f, -1.8f, -0.2f);
							if (!player.fall_to_walkmesh) player.transform->position = glm::mix(start, end, alpha);
						}, [&](){
							player.animation_lock_look = false;
							player.animation_lock_move = false;
						});
					
				});
			};
		}
		else if (b.name == "Lever") {
			b.on_pressed = [&](){
				scene.portals["HallExit"]->active = true;
				b.drawable->transform->rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(1,0,0));
				b.active = false;
			};
		}
	}


	//Screen Shader and quad Initialization
	Shader screenShader(data_path("shaders/screen.vs"), data_path("shaders/screen.fs"));
	screenShader.use();
	screenShader.setInt("screenTexture", 0);
	screenShader.setInt("normalTexture", 1);
	screenShader.setInt("depthTexture", 2);
	screenShaderID = screenShader.ID;
	InitQuadBuffers();

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
		} else if (evt.key.keysym.sym == SDLK_LSHIFT) {
			shift.downs += 1;
			shift.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_SPACE) {
			space.downs += 1;
			space.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_F3) {
			debug_menu.downs += 1;
			debug_menu.pressed = true;
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
		} else if (evt.key.keysym.sym == SDLK_SPACE) {
			space.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_F3) {
			debug_menu.pressed = false;
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
			return true;
		} else {
			if (evt.button.button == SDL_BUTTON_LEFT) {
				click.downs += 1;
				click.pressed = true;
				return true;
			}
		}
	} else if (evt.type == SDL_MOUSEBUTTONUP) {
		if (evt.button.button == SDL_BUTTON_LEFT) {
			click.pressed = false;
			return true;
		}
	} else if (evt.type == SDL_MOUSEMOTION) {
		if (SDL_GetRelativeMouseMode() == SDL_TRUE && !player.animation_lock_look) {
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
	//used for intro
	std::cout << player.transform->position.x << "," <<player.transform->position.y <<"," << player.transform->position.z <<std::endl;
	if (player.fall_to_walkmesh) {
		player.velocity.x = 0;
		player.velocity.y = 0;
		player.velocity.z -= player.gravity * elapsed;
		glm::vec3 new_pos = player.transform->position + player.velocity * elapsed;
		WalkPoint nearest_walk_point = walkmesh->nearest_walk_point(player.transform->position);
		if (new_pos.z <= walkmesh->to_world_point(nearest_walk_point).z) {
			player.fall_to_walkmesh = false;
			player.uses_walkmesh = true;
			player.at = nearest_walk_point;
		}
		else {
			player.transform->position = new_pos;
		}
	}

	//interaction
	constexpr float PlayerInteractRange = 1.7f;
	if (click.pressed && !click.last_pressed) {

		// ray box intersection from zacharmarz's answer: https://gamedev.stackexchange.com/questions/18436/most-efficient-aabb-vs-ray-collision-algorithms
		glm::mat4 const &cam_to_world = player.camera->transform->make_local_to_world();
		glm::vec4 const &cam_invdir = glm::vec4(glm::vec3(1.0f / -cam_to_world[2]), 0);
		glm::vec4 const &cam_origin = cam_to_world[3];
		auto bbox_hit = [&PlayerInteractRange](Scene::BoxCollider box, glm::vec3 invdir, glm::vec3 ray_origin){

			float t1 = (box.min.x - ray_origin.x)*invdir.x;
			float t2 = (box.max.x - ray_origin.x)*invdir.x;
			float t3 = (box.min.y - ray_origin.y)*invdir.y;
			float t4 = (box.max.y - ray_origin.y)*invdir.y;
			float t5 = (box.min.z - ray_origin.z)*invdir.z;
			float t6 = (box.max.z - ray_origin.z)*invdir.z;

			float tmin = glm::max(glm::max(glm::min(t1,t2), glm::min(t3,t4)), glm::min(t5,t6));
			float tmax = glm::min(glm::min(glm::max(t1,t2), glm::max(t3,t4)), glm::max(t5,t6));

			std::cout << tmin << ", " << tmax << "\n";

			if (tmax < 0) return false;
			if (tmin > tmax) return false;
			if (tmin > PlayerInteractRange) return false;

			return true;
		};

		for (auto &b : scene.buttons) {
			if (!b.active) continue;
			glm::mat4x3 const &b_to_local = b.drawable->transform->make_world_to_local();
			if (bbox_hit(b.box, b_to_local * cam_invdir, b_to_local * cam_origin)) {
				if (b.on_pressed) b.on_pressed();
				break;
			}
		}
	}


	//player walking:
	if (!player.animation_lock_move) {
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
			//player.transform->position += remain;
		}
	}

	//debug stuff
	frame_delta = elapsed;
	if (debug_menu.pressed && !debug_menu.last_pressed) {
		draw_debug = !draw_debug;
	}
	if (up_arrow.pressed && !up_arrow.last_pressed) {
		scene.default_draw_recursion_max += 1;
		std::cout << "Max draw recursion lvl: " << scene.default_draw_recursion_max << "\n";
	}
	if (down_arrow.pressed && !down_arrow.last_pressed && scene.default_draw_recursion_max > 0) {
		scene.default_draw_recursion_max -= 1;
		std::cout << "Max draw recursion lvl: " << scene.default_draw_recursion_max << "\n";
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
		space.downs = 0;
		debug_menu.downs = 0;
		up_arrow.downs = 0;
		down_arrow.downs = 0;

		//and adjust last_pressed:
		left.last_pressed = left.pressed;
		right.last_pressed = right.pressed;
		up.last_pressed = up.pressed;
		down.last_pressed = down.pressed;
		shift.last_pressed = shift.pressed;
		click.last_pressed = click.pressed;
		space.last_pressed = space.pressed;
		debug_menu.last_pressed = debug_menu.pressed;
		up_arrow.last_pressed = up_arrow.pressed;
		down_arrow.last_pressed = down_arrow.pressed;
	}

	handle_portals();

	for (auto &t : timers) {
		if (t.active) {
			t.tick(elapsed);
		}
	}
}

void PlayMode::handle_portals() {

	auto point_in_box = [](glm::vec3 x, glm::vec3 min, glm::vec3 max){
		return (min.x <= x.x && x.x <= max.x) && (min.y <= x.y && x.y <= max.y) && (min.z <= x.z && x.z <= max.z);
	};

	for (auto const &pair : scene.portals) {
		Scene::Portal *p = pair.second;
		if (p->dest == nullptr) continue;
		if (!p->active) continue;
		glm::mat4 p_world = p->drawable->transform->make_local_to_world();

		glm::vec3 offset_from_portal = player.transform->position - glm::vec3(p_world * glm::vec4(0,0,0,1));
		bool now_in_front = 0 < glm::dot(offset_from_portal, glm::normalize(glm::vec3(p_world[1])));
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

			glm::mat4x3 const &dest_world = p->dest->drawable->transform->make_local_to_world();
			glm::mat4 const m_reverse = glm::mat4(dest_world) * glm::mat4(p_local);

			// SPECIAL CASES ----------------------------
			bool normal_tp = true;
			if (p == scene.portals["PortalFridge"]) {
				player.transform->position = m_reverse * glm::vec4(player.transform->position, 1) - glm::vec4(0, 1.8f, 1.8f, 0);
				player.transform->rotation = glm::angleAxis(glm::radians(180.0f), glm::vec3(0,0,1));
				player.velocity.z = -0.4f * player.gravity;
				player.camera->transform->rotation = glm::angleAxis(0.05f * glm::radians(180.0f), glm::vec3(1,0,0));
				walkmesh = walkmesh_map[p->dest->on_walkmesh];
				player.uses_walkmesh = false;
				player.fall_to_walkmesh = true;
				p->active = false;
				normal_tp = false;
			}
			else if (p == scene.portals["StairPortal0"]) {
				scene.portals["StairPortalA"]->dest = p;
				scene.portals["StairPortal1"]->active = false;
				scene.portals["StairPortal2"]->active = false;
				scene.portals["StairPortal3"]->active = false;

				scene.portals["StairPortalB"]->dest = scene.portals["StairPortal0b"];
				scene.portals["StairPortal1b"]->active = false;
				scene.portals["StairPortal2b"]->active = false;
				scene.portals["StairPortal3b"]->active = false;

			}
			else if (p == scene.portals["StairPortal1"]) {
				scene.portals["StairPortalA"]->dest = p;
				scene.portals["StairPortal0"]->active = false;
				scene.portals["StairPortal2"]->active = false;
				scene.portals["StairPortal3"]->active = false;

				scene.portals["StairPortalB"]->dest = scene.portals["StairPortal1b"];
				scene.portals["StairPortal0b"]->active = false;
				scene.portals["StairPortal2b"]->active = false;
				scene.portals["StairPortal3b"]->active = false;
			}
			else if (p == scene.portals["StairPortal2"]) {
				scene.portals["StairPortalA"]->dest = p;
				scene.portals["StairPortal0"]->active = false;
				scene.portals["StairPortal1"]->active = false;
				scene.portals["StairPortal3"]->active = false;

				scene.portals["StairPortalB"]->dest = scene.portals["StairPortal2b"];
				scene.portals["StairPortal0b"]->active = false;
				scene.portals["StairPortal1b"]->active = false;
				scene.portals["StairPortal3b"]->active = false;
			}
			else if (p == scene.portals["StairPortal3"]) {
				scene.portals["StairPortalA"]->dest = p;
				scene.portals["StairPortal0"]->active = false;
				scene.portals["StairPortal1"]->active = false;
				scene.portals["StairPortal2"]->active = false;

				scene.portals["StairPortalB"]->dest = scene.portals["StairPortal3b"];
				scene.portals["StairPortal0b"]->active = false;
				scene.portals["StairPortal1b"]->active = false;
				scene.portals["StairPortal2b"]->active = false;
			}
			else if (p == scene.portals["StairPortalA"]) {
				scene.portals["StairPortal0"]->active = true;
				scene.portals["StairPortal1"]->active = true;
				scene.portals["StairPortal2"]->active = true;
				scene.portals["StairPortal3"]->active = true;
			}
			else if (p == scene.portals["StairPortal0b"]) {
				scene.portals["StairPortalA"]->dest = scene.portals["StairPortal0"];
				scene.portals["StairPortal1"]->active = false;
				scene.portals["StairPortal2"]->active = false;
				scene.portals["StairPortal3"]->active = false;

				scene.portals["StairPortalB"]->dest = p;
				scene.portals["StairPortal1b"]->active = false;
				scene.portals["StairPortal2b"]->active = false;
				scene.portals["StairPortal3b"]->active = false;

			}
			else if (p == scene.portals["StairPortal1b"]) {
				scene.portals["StairPortalA"]->dest = scene.portals["StairPortal1"];
				scene.portals["StairPortal0"]->active = false;
				scene.portals["StairPortal2"]->active = false;
				scene.portals["StairPortal3"]->active = false;

				scene.portals["StairPortalB"]->dest = p;
				scene.portals["StairPortal0b"]->active = false;
				scene.portals["StairPortal2b"]->active = false;
				scene.portals["StairPortal3b"]->active = false;
			}
			else if (p == scene.portals["StairPortal2b"]) {
				scene.portals["StairPortalA"]->dest = scene.portals["StairPortal2"];
				scene.portals["StairPortal0"]->active = false;
				scene.portals["StairPortal1"]->active = false;
				scene.portals["StairPortal3"]->active = false;

				scene.portals["StairPortalB"]->dest = p;
				scene.portals["StairPortal0b"]->active = false;
				scene.portals["StairPortal1b"]->active = false;
				scene.portals["StairPortal3b"]->active = false;
			}
			else if (p == scene.portals["StairPortal3b"]) {
				scene.portals["StairPortalA"]->dest = scene.portals["StairPortal3"];
				scene.portals["StairPortal0"]->active = false;
				scene.portals["StairPortal1"]->active = false;
				scene.portals["StairPortal2"]->active = false;

				scene.portals["StairPortalB"]->dest = p;
				scene.portals["StairPortal0b"]->active = false;
				scene.portals["StairPortal1b"]->active = false;
				scene.portals["StairPortal2b"]->active = false;
			}
			else if (p == scene.portals["StairPortalB"]) {
				scene.portals["StairPortal0b"]->active = true;
				scene.portals["StairPortal1b"]->active = true;
				scene.portals["StairPortal2b"]->active = true;
				scene.portals["StairPortal3b"]->active = true;
			}

			else if (p == scene.portals["HallExit"]) {
				scene.portals["StairPortalA"]->active = false;
				scene.portals["StairPortalB"]->active = false;
				scene.portals["StairPortal0"]->active = false;
				scene.portals["StairPortal1"]->active = false;
				scene.portals["StairPortal2"]->active = false;
				scene.portals["StairPortal0b"]->active = false;
				scene.portals["StairPortal1b"]->active = false;
				scene.portals["StairPortal2b"]->active = false;
				scene.portals["StairPortal3b"]->active = false;

				scene.default_draw_recursion_max = 1;
			}

			else if (p == scene.portals["HallExit"]) {
				scene.portals["StairPortalA"]->active = false;
				scene.portals["StairPortalB"]->active = false;
				scene.portals["StairPortal0"]->active = false;
				scene.portals["StairPortal1"]->active = false;
				scene.portals["StairPortal2"]->active = false;
				scene.portals["StairPortal0b"]->active = false;
				scene.portals["StairPortal1b"]->active = false;
				scene.portals["StairPortal2b"]->active = false;
				scene.portals["StairPortal3b"]->active = false;
				scene.portals["Drop0"]->active = false;
				scene.portals["Drop1"]->active = false;
				scene.portals["EnterPlayground"]->active = true;

				scene.default_draw_recursion_max = 2;
			}
			else if (p == scene.portals["FunZone"]) {
				scene.portals["StairPortalA"]->active = true;
				scene.portals["StairPortalB"]->active = true;
				scene.portals["StairPortal0"]->active = true;
				scene.portals["StairPortal1"]->active = true;
				scene.portals["StairPortal2"]->active = true;
				scene.portals["StairPortal0b"]->active = true;
				scene.portals["StairPortal1b"]->active = true;
				scene.portals["StairPortal2b"]->active = true;
				scene.portals["StairPortal3b"]->active = true;
				scene.portals["Drop0"]->active = true;
				scene.portals["Drop1"]->active = true;

				scene.portals["EnterPlayground"]->active = false;

				scene.default_draw_recursion_max = 0;
			}
			else if (p == scene.portals["EnterPlayground"]) {
				scene.portals["FunZone"]->active = false;
			}



			// normal case below ------------------------

			if (normal_tp) {
				player.transform->position = m_reverse * glm::vec4(player.transform->position, 1);
				player.transform->rotation = m_reverse * glm::mat4(player.transform->rotation);
				p->dest->sleeping = true;

				walkmesh = walkmesh_map[p->dest->on_walkmesh];
				if (walkmesh != nullptr) {
					player.at = walkmesh->nearest_walk_point(player.transform->position);
				}
			}
			
		}
	}
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {

	//need to resize framebuffer if drawable size changes:
	if(drawable_size.x != drawableSize.x || drawable_size.y != drawableSize.y){
		drawableSize = drawable_size;
		InitFrameBuffer(drawableSize);
	}

	//update camera aspect ratio for drawable:
	player.camera->aspect = float(drawable_size.x) / float(drawable_size.y);;

	//Start writing to framebuffer
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	//glDisable(GL_ALPHA_TEST);
	glEnable(GL_DEPTH_TEST);
	glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
	glDrawBuffers(3, drawBuffers);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_LOCATION_vec3, 1, glm::value_ptr(glm::vec3(-6.0f, 0.0f, 2.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(0.2f, 0.2f, 0.2f)));
	glUseProgram(0);

	glClearColor(0.8f, 0.8f, 0.8f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	glm::mat4x3 const player_cam_world = player.camera->transform->make_local_to_world();

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

	// 2nd Pass(Only render the screen space quad)
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDisable(GL_DEPTH_TEST); // disable depth test so screen-space quad isn't discarded due to depth test.
	//glEnable(GL_ALPHA_TEST);
	// clear all relevant buffers
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f); // set clear color to white (not really necessary actually, since we won't be able to see behind the quad anyways)
	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(screenShaderID);
	glUniform1i(glGetUniformLocation(screenShaderID, "width"), drawable_size.x);
	glUniform1i(glGetUniformLocation(screenShaderID, "height"), drawable_size.y);

	glBindVertexArray(quadVAO);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, textureColorbuffer);	// use the color attachment texture as the texture of the quad plane
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, textureNormalbuffer);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, textureDepthbuffer);
	glDrawArrays(GL_TRIANGLES, 0, 6);

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
		lines.draw_text("Mouse, WASD, shift to run",
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text("Mouse, WASD, shift to run",
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		
		if (draw_debug) {
			std::string const &fps = std::to_string(int(glm::round(1.0f / frame_delta)));
			lines.draw_text(fps,
			glm::vec3(-aspect + 0.1f * H, 0.99 - H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
			lines.draw_text(fps,
			glm::vec3(-aspect + 0.1f * H + ofs, 0.99 - H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		}
	}
	GL_ERRORS();
}