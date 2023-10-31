#include "Mode.hpp"

#include "Scene.hpp"
#include "WalkMesh.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	void draw_recursive_portals(Scene::Camera camera, glm::vec4 clip_plane, GLint max_depth, GLint current_depth);

	void update_physics(float elapsed);

	//----- game state -----

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	//player info:
	struct Player {
		bool uses_walkmesh = false;
		WalkPoint at;
		//transform is at player's feet and will be yawed by mouse left/right motion:
		Scene::Transform *transform = nullptr;
		//camera is at player's head and will be pitched by mouse up/down motion:
		Scene::Camera *camera = nullptr;
	} player;

	struct Portal {
		Portal(Scene::Drawable *drawable_, Scene::BoxCollider box_) : Portal(drawable_, box_, nullptr) {}
		Portal(Scene::Drawable *drawable_, Scene::BoxCollider box_, Portal *twin_) : drawable(drawable_), box(box_), twin(twin_) {
			camera = new Scene::Camera(new Scene::Transform());
			if (twin != nullptr) twin->twin = this;
		}
		~Portal() {
			if (twin != nullptr) {
				twin->twin = nullptr;
			}
			if (camera != nullptr) delete camera->transform;
			delete camera;
		}
		Scene::Drawable const *drawable = nullptr;
		Portal *twin = nullptr;
		Scene::BoxCollider box;
		Scene::Camera *camera = nullptr;
		bool player_in_front = false;
		bool sleeping = false;

		glm::vec4 get_clipping_plane();
		glm::vec4 get_self_clip_plane();
	};

	Scene::Transform *debug_sphere = nullptr;

	std::vector<Portal*> portals;
};
