#pragma once
#include "Mode.hpp"

#include "Scene.hpp"
#include "Sound.hpp"
#include "WalkMesh.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>

#include <iostream>


struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	void handle_portals();

	//----- game state -----

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
		uint8_t last_pressed = 0; //useful for only doing things once on press / release
	} left, right, down, up, shift, click, hide_overlay, up_arrow, down_arrow;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	//player info:
	struct Player {
		bool uses_walkmesh = true;
		WalkPoint at;
		//transform is at player's feet and will be yawed by mouse left/right motion:
		Scene::Transform *transform = nullptr;
		//camera is at player's head and will be pitched by mouse up/down motion:
		Scene::Camera *camera = nullptr;

        bool show_mouse_prompt = false;
	} player;

    Scene::ScreenImage cursor;
    Scene::ScreenImage mouse_prompt;
    Scene::ScreenImage controls_hint;
    Scene::ScreenImage pause_text;

    bool paused = false;
    bool hide_all_overlays = false;

	float frame_delta = 0;

	std::unordered_map<std::string, WalkMesh const *> walkmesh_map;
	WalkMesh const *walkmesh = nullptr;

	//----- Random scripting objects -----

    Scene::Transform *rotate_base = nullptr;

    struct Timer {
		Timer(float time_, std::function<void(float)> on_tick_ = {}, std::function<void()> on_finish_ = {}, bool auto_start_ = true, bool auto_delete_ = true) :
			max_acc(time_), on_tick(on_tick_), on_finish(on_finish_), active(auto_start_), auto_delete(auto_delete_) {}
		~Timer() {}
		float max_acc = 1;
		float acc = 0;
		std::function<void(float)> on_tick = {};
		std::function<void()> on_finish = {};
		void tick(float elapsed) {
			acc += elapsed;
			if (acc > max_acc) {
				acc = max_acc;
				active = false;
				if (on_tick) on_tick(alpha());
				if (on_finish) on_finish();
				if (auto_delete) {
					queued_for_delete = true;
				}
			}
			else {
				if (on_tick) on_tick(alpha());
			}
		}
		float alpha() {
			return acc / max_acc;
		}

		bool active = false;

		bool auto_delete = true;
		bool queued_for_delete = false;
	};

	std::vector<Timer> timers = std::vector<Timer>();


};
