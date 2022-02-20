#include "experimental.hpp"

#include "graphics_engine/graphics_engine_commands.hpp"
#include "graphics_engine/graphics_engine.hpp"
#include "game_engine.hpp"
#include "hot_reload.hpp"

#include <iostream>


Experimental::Experimental(GameEngine& engine_) :
	engine(engine_)
{
	// engine.get_graphics_engine().enqueue_cmd(std::make_unique<SpawnObjectCmd>(v1));
	// engine.get_graphics_engine().enqueue_cmd(std::make_unique<SpawnObjectCmd>(v2));
	// engine.get_graphics_engine().enqueue_cmd(std::make_unique<SpawnObjectCmd>(v3));
	// engine.get_graphics_engine().enqueue_cmd(std::make_unique<SpawnObjectCmd>(v4));

	// v1.set_rotation(glm::angleAxis(Maths::PI, glm::vec3(0.0f, -1.0f, 0.0f)));
}

void Experimental::process()
{
	std::cout << "Experimental\n";
	HotReload::func3(v1);
}