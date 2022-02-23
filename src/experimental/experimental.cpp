#include "experimental.hpp"

#include "graphics_engine/graphics_engine_commands.hpp"
#include "graphics_engine/graphics_engine.hpp"
#include "game_engine.hpp"
#include "hot_reload.hpp"

#include <iostream>


Experimental::Experimental(GameEngine& engine_) :
	engine(engine_),
	cube1("../resources/textures/texture.jpg"),
	cube2("../resources/textures/texture.jpg")
{
	cube2.set_position(glm::vec3(1.0f, 0.0f, 0.0f));
	cube2.attach_to(&cube1);
	// engine.draw_object(cube1);
	// engine.draw_object(cube2);
}

void Experimental::process()
{
	std::cout << "Experimental\n";
}

void Experimental::process(float time_delta)
{
	return; // uncomment to enable

	// time_delta is in seconds
	glm::vec3 axis;
	HotReload::get().slfp->gen_vec(axis);
	glm::quat rotator = glm::angleAxis(0.02f, glm::normalize(axis));
	cube1.set_rotation(rotator * cube1.get_rotation());
}