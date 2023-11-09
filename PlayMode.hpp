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

	void draw_recursive_portals(glm::mat4 view_mat, glm::vec3 view_pos, glm::vec4 clip_plane, GLint max_depth, GLint current_depth);

	void handle_portals();

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
		bool uses_walkmesh = true;
		WalkPoint at;
		//transform is at player's feet and will be yawed by mouse left/right motion:
		Scene::Transform *transform = nullptr;
		//camera is at player's head and will be pitched by mouse up/down motion:
		Scene::Camera *camera = nullptr;
	} player;

	struct Portal {
		Portal(Scene::Drawable *drawable_, Scene::BoxCollider box_, std::string on_walkmesh_) : Portal(drawable_, box_, on_walkmesh_, nullptr) {}
		Portal(Scene::Drawable *drawable_, Scene::BoxCollider box_, std::string on_walkmesh_, Portal *twin_) : drawable(drawable_), box(box_.min + glm::vec3(0,-0.25f, 0), box_.max + glm::vec3(0,0.25f,0)), on_walkmesh(on_walkmesh_), twin(twin_) {
			if (twin != nullptr) twin->twin = this;
		}
		~Portal() {
			if (twin != nullptr) {
				twin->twin = nullptr;
			}
		}
		Scene::Drawable const *drawable = nullptr;
		Portal *twin = nullptr;
		Scene::BoxCollider box;
		bool player_in_front = false;
		bool sleeping = false;

		bool active = true;
		std::string on_walkmesh;

		glm::vec4 get_clipping_plane(glm::vec3 view_pos);
		glm::vec4 get_self_clip_plane(glm::vec3 view_pos);
	};

	std::unordered_map<std::string, WalkMesh const *> walkmesh_map;

	WalkMesh const *walkmesh = nullptr;

	std::unordered_map<std::string, Portal*> portals;

	bool last_hint_opened = false;
};
