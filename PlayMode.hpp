#include "Mode.hpp"

#include "Scene.hpp"
#include "Sound.hpp"

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

	//----- game state -----

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up, jump, yell;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	//models:
	Scene::Transform *sheep = nullptr;
	Scene::Transform *apple = nullptr;
	Scene::Transform *sheep_head = nullptr;
	glm::quat sheep_base_rotation;
	glm::quat apple_base_rotation;
	glm::quat sheep_head_base_rotation;
	glm::vec3 veclocity = glm::vec3(0.0f,0,0);
	float stamana = 0;
	float wobble = 0.0f;
	float cooldown = 0.0f;
	float applechange = 10.0f;
	int points=0;

	glm::vec3 get_sheep_head_position();

	//music coming from the tip of the leg (as a demonstration):
	std::shared_ptr< Sound::PlayingSample > sheepnoise;
	
	//camera:
	Scene::Camera *camera = nullptr;

};
