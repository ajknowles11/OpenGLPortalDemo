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
GLuint meshes_for_solid_outline_program = 0;
Load< MeshBuffer > level_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("level/level.pnct"));
	meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	meshes_for_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	meshes_for_solid_outline_program = ret->make_vao_for_program(solid_outline_program->program);
	return ret;
});

Load< Scene > level_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("level/level.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = level_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		if (mesh.program == 0) {
			drawable.pipeline = solid_outline_program_pipeline;
			drawable.pipeline.vao = meshes_for_solid_outline_program;
		}
		else if (mesh.program == 1) {
			drawable.pipeline = lit_color_texture_program_pipeline;
			drawable.pipeline.vao = meshes_for_lit_color_texture_program;
		}


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

		if (mesh.program == 0) {
			drawable.pipeline = solid_outline_program_pipeline;
			drawable.pipeline.vao = meshes_for_solid_outline_program;
		}
		else if (mesh.program == 1) {
			drawable.pipeline = lit_color_texture_program_pipeline;
			drawable.pipeline.vao = meshes_for_lit_color_texture_program;
		}
		
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

// textures ------------------

Load< Scene::Texture > cursor_texture(LoadTagDefault, []() -> Scene::Texture const * {
	return new Scene::Texture(data_path("textures/cursor.png"));
});

Load< Scene::Texture > mouse_texture(LoadTagDefault, []() -> Scene::Texture const * {
	return new Scene::Texture(data_path("textures/mouse.png"));
});

Load< Scene::Texture > hint0_texture(LoadTagDefault, []() -> Scene::Texture const * {
	return new Scene::Texture(data_path("textures/hint0.png"));
});

Load< Scene::Texture > hint_texture(LoadTagDefault, []() -> Scene::Texture const * {
	return new Scene::Texture(data_path("textures/hint.png"));
});

Load< Scene::Texture > controls_texture(LoadTagDefault, []() -> Scene::Texture const * {
	return new Scene::Texture(data_path("textures/controls.png"));
});

Load< Scene::Texture > pause_texture(LoadTagDefault, []() -> Scene::Texture const * {
	return new Scene::Texture(data_path("textures/pause.png"));
});

// audio ---------------------

Load< Sound::Sample > lever_on(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("sfx/lever-open.opus"));
});

Load< Sound::Sample > lever_off(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("sfx/lever-close.opus"));
});

Load< Sound::Sample > fridge_open(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("sfx/fridge-open.opus"));
});

Load< Sound::Sample > ambient(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("bgm/AmbientBGM.opus"));
});

Load< Sound::Sample > fallsfx(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("sfx/fall.opus"));
});

Load< Sound::Sample > homebgm(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("bgm/home.opus"));
});

Load< Sound::Sample > boomsfx(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("sfx/boom.opus"));
});

Load< Sound::Sample > run1sfx(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("sfx/footstep/run-L-1.opus"));
});

Load< Sound::Sample > run2sfx(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("sfx/footstep/run-L-2.opus"));
});

Load< Sound::Sample > run3sfx(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("sfx/footstep/run-L-3.opus"));
});

Load< Sound::Sample > run4sfx(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("sfx/footstep/run-R-1.opus"));
});

Load< Sound::Sample > walk1sfx(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("sfx/footstep/walk-L-1.opus"));
});

Load< Sound::Sample > walk2sfx(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("sfx/footstep/walk-L-2.opus"));
});

Load< Sound::Sample > walk3sfx(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("sfx/footstep/walk-L-3.opus"));
});

Load< Sound::Sample > walk4sfx(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("sfx/footstep/walk-R-1.opus"));
});


// ---------------------------

PlayMode::PlayMode() : scene(*level_scene) {
	scene.full_tri_program = *full_tri_program;

	//create a player transform:
	scene.transforms.emplace_back();
	player.transform = &scene.transforms.back();
	player.transform->position = glm::vec3(-2.0f, 0.0f, 0.0f);
	player.transform->rotation = glm::quat(glm::vec3(0.0f, 0.0f, glm::radians(-70.0f)));

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
	player.camera->transform->rotation = glm::quat(glm::vec3(glm::radians(70.0f), 0.0f, 0.0f));

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

	scene.current_group = &scene.portal_groups["Start"];

	scene.portals["HallExit"]->active = false;
	scene.portals["FlipExit"]->active = false;
	scene.portals["DeacHard1"]->active = false;
	scene.portals["DeacHard3"]->active = false;

	for (auto &t : scene.transforms) {
		if (t.name == "RotateBase") {
			rotate_base = &t;
		}
	}
	

	//Button scripting
	for (auto &b : scene.buttons) {
		if (b.name == "FridgeDoor") {
			b.on_pressed = [&](){
				if (!intro_done) return; //don't let people continue unless we showed them all hint text
				b.active = false;
				home_sample->stop(0.5f);
				Sound::play_3D(*fridge_open, 1.0f, b.drawable->transform->make_local_to_world()[3], 10.0f);
				milk_hint_count = 0;
				controls_hint_show = false;
				player.animation_lock_move = true;
				player.animation_lock_look = true;
				player.uses_walkmesh = false;
				Sound::play(*fallsfx, 1.5f);
				timers.emplace_back(1.2f, [&](float alpha){
						glm::vec3 const &target = glm::vec3(-4.5f, 0.5f, -0.2f);
						player.transform->position = glm::mix(player.transform->position, target, alpha);
						float yaw = glm::roll(player.transform->rotation); //glm moment
						if (yaw < 0) yaw += glm::radians(360.0f);
						player.transform->rotation = glm::quat(glm::vec3(0.0f, 0.0f, glm::mix(yaw, glm::radians(180.0f), alpha))); 
						player.camera->transform->rotation = glm::quat(glm::vec3(glm::mix(glm::pitch(player.camera->transform->rotation), 1.1f * glm::radians(90.0f), alpha), 0, 0));
						b.drawable->transform->rotation = glm::lerp(glm::quat(), glm::angleAxis(glm::radians(90.0f), glm::vec3(0,0,1)), (alpha * 2.0f > 1.0f ? 1.0f : alpha * 2.0f));
					}, [&](){
						timers.emplace_back(0.35f, [&](float alpha){
							glm::vec3 const &start = glm::vec3(-4.5f, 0.5f, -0.2f);
							glm::vec3 const &end = glm::vec3(-4.5f, -1.8f, -0.2f);
							if (!player.fall_to_walkmesh) player.transform->position = glm::mix(start, end, alpha);
						}, [&](){
							player.animation_lock_look = false;
							player.animation_lock_move = false;

							// TELEPORT PLAYER (MANUALLY INSTEAD OF USING PORTAL LOGIC)

							Scene::Portal *p = scene.portals["PortalFridge"];
							scene.current_group = &scene.portal_groups[p->dest->group];

							player.transform->position = p->dest->drawable->transform->position + glm::vec3(0, -1.6f, -1.8f);
							player.transform->rotation = glm::angleAxis(glm::radians(180.0f), glm::vec3(0,0,1));
							player.velocity.z = -0.4f * player.gravity;
							player.camera->transform->rotation = glm::angleAxis(0.05f * glm::radians(180.0f), glm::vec3(1,0,0));
							walkmesh = walkmesh_map[p->dest->on_walkmesh];
							player.uses_walkmesh = false;
							player.fall_to_walkmesh = true;
							p->active = false;
						});
					
				});
			};
		}
		else if (b.name == "Lever") {
			b.on_pressed = [&](){
				b.hit = !b.hit;
				b.active = false;
				scene.portals["HallExit"]->active = b.hit; //we used to activate after lever anim finished, but could walk through deac'd portals quickly in some cases.
				//we could also init timer once (without auto start/delete) and just save a ref, but it's not a big deal
				timers.emplace_back(0.12f, [&](float alpha){
					float angle = b.hit ? glm::mix(0.0f, 90.0f, alpha) : glm::mix(90.0f, 0.0f, alpha);
					b.drawable->transform->rotation = glm::angleAxis(glm::radians(angle), glm::vec3(1,0,0));
				}, [&](){
					if (b.hit) {
						Sound::play_3D(*lever_on, 0.8f, b.drawable->transform->make_local_to_world()[3], 10.0f);
					}
					else {
						Sound::play_3D(*lever_off, 0.8f, b.drawable->transform->make_local_to_world()[3], 10.0f);
					}
					b.active = true;
				});
			};
		}
		else if (b.name == "LeverFlip") {
			b.on_pressed = [&](){
				b.hit = !b.hit;
				b.active = false;
				scene.portals["FlipExit"]->active = b.hit;
				timers.emplace_back(0.12f, [&](float alpha){
					float angle = b.hit ? glm::mix(0.0f, -90.0f, alpha) : glm::mix(-90.0f, 0.0f, alpha);
					b.drawable->transform->rotation = glm::angleAxis(glm::radians(angle), glm::vec3(1,0,0));
				}, [&](){
					if (b.hit) {
						Sound::play_3D(*lever_on, 0.8f, b.drawable->transform->make_local_to_world()[3], 10.0f);
					}
					else {
						Sound::play_3D(*lever_off, 0.8f, b.drawable->transform->make_local_to_world()[3], 10.0f);
					}
					b.active = true;
				});
			};
		}
		else if (b.name == "LeverDeac") {
			b.hit = true;
			b.on_pressed = [&](){
				b.hit = !b.hit;
				b.active = false;
				scene.portals["Deac1"]->active = b.hit;
				scene.portals["Deac2"]->active = b.hit;
				timers.emplace_back(0.12f, [&](float alpha){
					float angle = b.hit ? glm::mix(90.0f, 0.0f, alpha) : glm::mix(0.0f, 90.0f, alpha);
					b.drawable->transform->rotation = glm::angleAxis(glm::radians(angle), glm::vec3(0,1,0));
				}, [&](){
					if (b.hit) {
						Sound::play_3D(*lever_on, 0.8f, b.drawable->transform->make_local_to_world()[3], 10.0f);
					}
					else {
						Sound::play_3D(*lever_off, 0.8f, b.drawable->transform->make_local_to_world()[3], 10.0f);
					}
					b.active = true;
				});
			};
		}
		else if (b.name == "LeverDeac2") {
			b.hit = true;
			b.on_pressed = [&](){
				b.hit = !b.hit;
				b.active = false;
				scene.portals["Deac3"]->active = b.hit;
				scene.portals["Deac4"]->active = b.hit;
				timers.emplace_back(0.12f, [&](float alpha){
					float angle = b.hit ? glm::mix(90.0f, 0.0f, alpha) : glm::mix(0.0f, 90.0f, alpha);
					b.drawable->transform->rotation = glm::angleAxis(glm::radians(angle), glm::vec3(0,0,1));
				}, [&](){
					if (b.hit) {
						Sound::play_3D(*lever_on, 0.8f, b.drawable->transform->make_local_to_world()[3], 10.0f);
					}
					else {
						Sound::play_3D(*lever_off, 0.8f, b.drawable->transform->make_local_to_world()[3], 10.0f);
					}
					b.active = true;
				});
			};
		}
		else if (b.name == "LeverDeac3") {
			b.on_pressed = [&](){
				b.hit = !b.hit;
				b.active = false;
				scene.portals["DeacHard1"]->active = b.hit;
				scene.portals["DeacHard3"]->active = b.hit;
				timers.emplace_back(0.12f, [&](float alpha){
					float angle = b.hit ? glm::mix(0.0f, 90.0f, alpha) : glm::mix(90.0f, 0.0f, alpha);
					b.drawable->transform->rotation = glm::angleAxis(glm::radians(angle), glm::vec3(1,0,0));
				}, [&](){
					if (b.hit) {
						Sound::play_3D(*lever_on, 0.8f, b.drawable->transform->make_local_to_world()[3], 10.0f);
					}
					else {
						Sound::play_3D(*lever_off, 0.8f, b.drawable->transform->make_local_to_world()[3], 10.0f);
					}
					b.active = true;
				});
			};
		}
		else if (b.name == "LeverDeac4") {
			b.hit = true;
			b.on_pressed = [&](){
				b.hit = !b.hit;
				b.active = false;
				scene.portals["DeacHard0"]->active = b.hit;
				scene.portals["DeacHard2"]->active = b.hit;
				timers.emplace_back(0.12f, [&](float alpha){
					float angle = b.hit ? glm::mix(-90.0f, 0.0f, alpha) : glm::mix(0.0f, -90.0f, alpha);
					b.drawable->transform->rotation = glm::angleAxis(glm::radians(angle), glm::vec3(0,1,0));
				}, [&](){
					if (b.hit) {
						Sound::play_3D(*lever_on, 1.0f, b.drawable->transform->make_local_to_world()[3], 10.0f);
					}
					else {
						Sound::play_3D(*lever_off, 1.0f, b.drawable->transform->make_local_to_world()[3], 10.0f);
					}
					b.active = true;
				});
			};
		}
		else if (b.name == "RotateB") {
			b.range_mult = 0.75f;
			b.on_pressed = [&](){
				b.hit = !b.hit;
				b.active = false;
				float const &zpos = b.drawable->transform->position.z;
				// timer for button press, which makes timers for depress and portal rotation
				timers.emplace_back(0.1f, [&, zpos](float alpha){ // press anim
					b.drawable->transform->position.z = glm::mix(zpos, zpos - 0.2f, alpha);
				}, [&, zpos](){ // start other anims when press done
					timers.emplace_back(0.15f, [&](float alpha){ // depress anim
						b.drawable->transform->position.z = glm::mix(zpos - 0.2f, zpos, alpha);
					});
					timers.emplace_back(1.5f, [&](float alpha){ // rotate anim
						if (b.hit) {
							rotate_base->rotation = glm::angleAxis(glm::mix(glm::radians(0.0f), glm::radians(180.0f), alpha), glm::vec3(0,0,1));
						}
						else {
							rotate_base->rotation = glm::angleAxis(glm::mix(glm::radians(180.0f), glm::radians(360.0f), alpha), glm::vec3(0,0,1));
						}
					}, [&](){ // finish
						b.active = true;
					});
				});
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

	//ScreenImage UI elements
	cursor = Scene::ScreenImage(*cursor_texture, glm::vec2(0), glm::vec2(0.0125f), Scene::ScreenImage::Center, color_texture_program);
	mouse_prompt = Scene::ScreenImage(*mouse_texture, glm::vec2(0), glm::vec2(0.04f), Scene::ScreenImage::Center, color_texture_program);

	constexpr float milk_hint_width = 0.3f * 128.0f / 45.0f;

	milk_hint0 = Scene::ScreenImage(*hint0_texture, glm::vec2(0.0f, -0.9f), glm::vec2(milk_hint_width, 0.3f), Scene::ScreenImage::Bottom, color_texture_program);
	milk_hint = Scene::ScreenImage(*hint_texture, glm::vec2(0.0f, -0.9f), glm::vec2(milk_hint_width, 0.3f), Scene::ScreenImage::Bottom, color_texture_program);

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
			// start playing if first time
			if (!started_playing) {
				started_playing = true;
				home_sample = Sound::loop(*homebgm, 0.2f);
				timers.emplace_back(5.5f, [&](float alpha){
					if (alpha >= 3.5f / 5.5f) {
						if (milk_hint_count < 2) {
							milk_hint_count = 2;
							Sound::play(*boomsfx, 0.5f);
						}
					}
					else if (alpha >= 1.5f / 5.5f) {
						if (milk_hint_count < 1) {
							milk_hint_count = 1;
							Sound::play(*boomsfx, 0.5f);
						}
					}
				}, [&](){
					controls_hint_show = true;
					intro_done = true;
				});
			}
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
	if (paused) return;
	//used for intro
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
			ambient_sample = Sound::loop(*ambient, 0.0f);
			timers.emplace_back(2.0f, [&](float alpha){
				ambient_sample->volume = glm::mix(0.0f, 0.4f, alpha);
			});
		}
		else {
			player.transform->position = new_pos;
		}
	}

	//interaction with buttons
	constexpr float MaxButtonPollRange2 = 100.0f; //speed up checking a bit by ignoring far buttons
	constexpr float PlayerInteractRange = 1.7f;
	{
		// ray box intersection from zacharmarz's answer: https://gamedev.stackexchange.com/questions/18436/most-efficient-aabb-vs-ray-collision-algorithms
		glm::mat4 const &cam_to_world = player.camera->transform->make_local_to_world();
		glm::vec4 const &cam_invdir = glm::vec4(glm::vec3(1.0f / -cam_to_world[2]), 0);
		glm::vec4 const &cam_origin = cam_to_world[3];
		auto bbox_hit = [&PlayerInteractRange](Scene::BoxCollider box, glm::vec3 invdir, glm::vec3 ray_origin, float range_mult){

			float t1 = (box.min.x - ray_origin.x)*invdir.x;
			float t2 = (box.max.x - ray_origin.x)*invdir.x;
			float t3 = (box.min.y - ray_origin.y)*invdir.y;
			float t4 = (box.max.y - ray_origin.y)*invdir.y;
			float t5 = (box.min.z - ray_origin.z)*invdir.z;
			float t6 = (box.max.z - ray_origin.z)*invdir.z;

			float tmin = glm::max(glm::max(glm::min(t1,t2), glm::min(t3,t4)), glm::min(t5,t6));
			float tmax = glm::min(glm::min(glm::max(t1,t2), glm::max(t3,t4)), glm::max(t5,t6));

			if (tmax < 0) return false;
			if (tmin > tmax) return false;
			if (tmin > PlayerInteractRange * range_mult) return false;

			return true;
		};

		player.show_mouse_prompt = false;
		for (auto &b : scene.buttons) {
			if (!b.active) continue;
			if (glm::distance2(glm::vec3(cam_origin), b.drawable->transform->make_local_to_world()[3]) > MaxButtonPollRange2) continue;
			glm::mat4x3 const &b_to_local = b.drawable->transform->make_world_to_local();
			if (bbox_hit(b.box, b_to_local * cam_invdir, b_to_local * cam_origin, b.range_mult)) {
				player.show_mouse_prompt = true;
				if (click.pressed && !click.last_pressed && b.on_pressed) b.on_pressed();
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
		
		footstep_acc += glm::length(move);
		if (footstep_acc >= footstep_time) {
			footstep_acc = 0;
			int index = std::rand() % 4;
			if (shift.pressed) {
				static Sound::Sample const *arr[4] = {run1sfx, run2sfx, run3sfx, run4sfx};
				Sound::play(*arr[index], 0.35f);
			}
			else {
				static Sound::Sample const *arr[4] = {walk1sfx, walk2sfx, walk3sfx, walk4sfx};
				Sound::play(*arr[index], 0.2f);
			}
		}

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
			if (p == scene.portals["StairPortal0"]) {
				scene.portals["StairPortalA"]->dest = p;
				scene.portals["StairPortal1"]->active = false;
				scene.portals["StairPortal2"]->active = false;
				scene.portals["StairPortal3"]->active = false;

				scene.portals["StairPortalB"]->dest = scene.portals["StairPortal0b"];
				scene.portals["StairPortal1b"]->active = false;
				scene.portals["StairPortal2b"]->active = false;
				scene.portals["StairPortal3b"]->active = false;
				scene.default_draw_recursion_max = 1;

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
				scene.default_draw_recursion_max = 1;
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
				scene.default_draw_recursion_max = 1;
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
				scene.default_draw_recursion_max = 1;
			}
			else if (p == scene.portals["StairPortalA"]) {
				scene.portals["StairPortal0"]->active = true;
				scene.portals["StairPortal1"]->active = true;
				scene.portals["StairPortal2"]->active = true;
				scene.portals["StairPortal3"]->active = true;
				scene.default_draw_recursion_max = 0;
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
				scene.default_draw_recursion_max = 1;

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
				scene.default_draw_recursion_max = 1;
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
				scene.default_draw_recursion_max = 1;
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
				scene.default_draw_recursion_max = 1;
			}
			else if (p == scene.portals["StairPortalB"]) {
				scene.portals["StairPortal0b"]->active = true;
				scene.portals["StairPortal1b"]->active = true;
				scene.portals["StairPortal2b"]->active = true;
				scene.portals["StairPortal3b"]->active = true;
				scene.default_draw_recursion_max = 0;
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

				scene.portals["FunZone"]->active = true;

				ambient_sample->volume = 0.15f;

				scene.default_draw_recursion_max = 1;
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

				scene.portals["FunZone"]->active = false;

				ambient_sample->volume = 0.4f;

				scene.default_draw_recursion_max = 0;
			}



			// normal case below ------------------------

			if (normal_tp) {
				player.transform->position = m_reverse * glm::vec4(player.transform->position, 1);
				player.transform->rotation = m_reverse * glm::mat4(player.transform->rotation);
				p->dest->sleeping = true;

				scene.current_group = &scene.portal_groups[p->dest->group];

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
	player.camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//Start writing to framebuffer
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	//glDisable(GL_ALPHA_TEST);
	glEnable(GL_DEPTH_TEST);
	glClearColor(0.8f, 0.8f, 0.8f, 1.0f);
	glClearDepth(1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT); 
	GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
	glDrawBuffers(3, drawBuffers);

	GL_ERRORS();

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

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
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);

	// now draw UI elements
	float aspect = float(drawable_size.x) / float(drawable_size.y);

	// cursor
	if (player.show_mouse_prompt) {
		mouse_prompt.draw(aspect);
	}
	else {
		cursor.draw(aspect);
	}

	if (milk_hint_count == 1) {
		milk_hint0.draw(aspect);
	}
	else if (milk_hint_count == 2) {
		milk_hint.draw(aspect);
	}

	if (controls_hint_show) {
		controls_hint.draw(aspect);
	}

	if (paused) {
		pause_text.draw(aspect);
	}

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

		if (draw_debug) {
			std::string const &fps = std::to_string(int(glm::round(1.0f / frame_delta)));
			lines.draw_text(fps,
			glm::vec3(-aspect + 0.1f * H, 0.99f - H, 0.0f),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
			lines.draw_text(fps,
			glm::vec3(-aspect + 0.1f * H + ofs, 0.99f - H + ofs, 0.0f),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		}
	}
	GL_ERRORS();
}