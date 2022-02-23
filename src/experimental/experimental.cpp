#include "experimental.hpp"

#include "graphics_engine/graphics_engine_commands.hpp"
#include "graphics_engine/graphics_engine.hpp"
#include "game_engine.hpp"
#include "hot_reload.hpp"
#include "objects/objects.hpp"

#include <iostream>


Experimental::Experimental(GameEngine& engine_) :
	engine(engine_)
{
	auto& arc = engine.spawn_object<Arc>();
}

void Experimental::process()
{
	std::cout << "Experimental\n";
}

void Experimental::process(float time_delta)
{
	// time_delta is in seconds

	return; // uncomment to enable

	// glm::vec3 axis;
	// HotReload::get().slfp->gen_vec(axis);
	// glm::quat rotator = glm::angleAxis(0.02f, glm::normalize(axis));
	// cube1.set_rotation(rotator * cube1.get_rotation());
}