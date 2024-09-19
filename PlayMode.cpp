#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <random>

GLuint grass_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > grass_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("grass.pnct"));
	grass_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > grass_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("grass.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = grass_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = grass_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});
// bkgd music from beepbox
Load< Sound::Sample > bkgd_music(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("backgroundmusic.wav"));
});

// sheep noises from my friends
Load< Sound::Sample > sheep1_noise(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("sheep1.wav"));
});

Load< Sound::Sample > sheep2_noise(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("sheep2.wav"));
});

Load< Sound::Sample > sheep3_noise(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("sheep3.wav"));
});
// from jfxr
Load< Sound::Sample > jump_sound(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("jump.wav"));
});

Load< Sound::Sample > pickup_sound(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("pickup.wav"));
});

PlayMode::PlayMode() : scene(*grass_scene) {
	//get pointers to leg for convenience:
	for (auto &transform : scene.transforms) {
		if (transform.name == "sheep") sheep = &transform;
		else if (transform.name == "apple") apple = &transform;
		else if (transform.name == "head") sheep_head = &transform;
	}
	if (sheep == nullptr) throw std::runtime_error("sheep not found.");
	if (apple == nullptr) throw std::runtime_error("apple not found.");
	if (sheep_head == nullptr) throw std::runtime_error("sheep head not found.");

	sheep_base_rotation = sheep->rotation;
	apple_base_rotation = apple->rotation;
	sheep_head_base_rotation = sheep_head->rotation;

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();

	sheep->position = glm::vec3(0.0f,0.0f,5);
	//background music loop playing:
	Sound::loop(*bkgd_music, .3f, false);
	//update listener to camera position:
	glm::mat4x3 frame = camera->transform->make_local_to_parent();
	glm::vec3 frame_right = frame[0];
	glm::vec3 frame_at = frame[3];
	Sound::listener.set_position_right(frame_at, frame_right, 1.0f / 60.0f);
}

PlayMode::~PlayMode() {
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
		} else if (evt.key.keysym.sym == SDLK_f) {
			// noise
			yell.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_SPACE) {
			// jump
			jump.pressed = true;
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
		} else if (evt.key.keysym.sym == SDLK_f) {
			yell.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_SPACE) {
			jump.pressed = false;
			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {

	stamana+=elapsed*.7f;
	stamana = std::min(stamana,20.0f); // cap it 
	cooldown=std::max(cooldown - elapsed,0.0f);
	applechange-=elapsed;

	//slowly rotates through [0,1):
	// wobble += elapsed / 10.0f;
	// wobble -= std::floor(wobble);
	// maybe have grass wobble -- needs shearing... :((

	//move sheep:
	//combine inputs into a move:
	constexpr float PlayerSpeed = 5.0f;
	constexpr float gravity = 10.0f;
	float angle = 0;
	glm::vec2 move = glm::vec2(0.0f);
	if (left.pressed && !right.pressed) {move.x =-1.0f; angle = 0;}
	if (!left.pressed && right.pressed) {move.x = 1.0f; angle = 3.14f;}
	if (down.pressed && !up.pressed) {move.y =-1.0f; angle = 3.14f/2;}
	if (!down.pressed && up.pressed) {move.y = 1.0f; angle = 1.5f*3.14f;}

	//make it so that moving diagonally doesn't go faster:
	if (move != glm::vec2(0.0f)) move = glm::normalize(move) * PlayerSpeed * elapsed;

	sheep->position += glm::vec3(move.x, move.y,0) ;
	sheep->rotation = sheep_base_rotation * glm::angleAxis( angle, glm::vec3(0.0f, 0.0f, 1.0f));
	
	if (jump.pressed){
		if (stamana>10){
		veclocity = glm::vec3(0.0f,0,stamana*.5+8);
		stamana -= 10;
		Sound::play_3D(*jump_sound, 3.0f, sheep->position, 1.0f);
		}
	}
	veclocity.z=(veclocity.z-gravity* elapsed);
	sheep->position.z=std::max(0.0f,(sheep->position+veclocity* elapsed).z);

	// limit to bounds of the field
	sheep->position.x=std::max(-20.0f,sheep->position.x);
	sheep->position.x=std::min(20.0f,sheep->position.x);
	sheep->position.y=std::max(-20.0f,sheep->position.y);
	sheep->position.y=std::min(20.0f,sheep->position.y);

	if(yell.pressed){
		if (cooldown==0.0f){
			cooldown = 0.70f;
			int i = std::rand()%3;
			if (i==0){
				Sound::play_3D(*sheep1_noise, 10.0f, get_sheep_head_position(), .20f);}
			else if (i==1){
				Sound::play_3D(*sheep2_noise, 30.0f, get_sheep_head_position(), .20f);}
			else{
				Sound::play_3D(*sheep3_noise, 60.0f, get_sheep_head_position(), .20f);}
			
		}
	}

	// check if apple was collected
	if (glm::length(sheep->position - apple->position)<3.0f){
		applechange = 0.0f;
		points++;
		stamana+=3;
		Sound::play_3D(*pickup_sound, 3.0f, sheep->position, 1.0f);
	}

	if (applechange<=0){
		applechange = 15;
		apple->position = glm::vec3((std::rand()%40)-20.0f,(std::rand()%40)-20.0f,10.0f);
	}
	apple->rotation*= glm::angleAxis( 3*elapsed, glm::vec3(0.0f, 0.0f, 1.0f));

	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	scene.draw(*camera);

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
		lines.draw_text("      Stamana: "+ std::to_string((int)stamana) + 
		"                                                                       "+ 
		"Points: "+std::to_string(points),
			glm::vec3(-aspect + 0.1f * H,  .9, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text("      Stamana: "+ std::to_string((int)stamana) + 
		"                                                                       "+ 
		 "Points: "+std::to_string(points),
			glm::vec3(-aspect + 0.1f * H + ofs,  .9 + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
	}
	GL_ERRORS();
}

glm::vec3 PlayMode::get_sheep_head_position() {
	return sheep_head->make_local_to_world() * glm::vec4(1.0f,1,1.0f, 1.0f);
}