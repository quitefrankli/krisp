#pragma once

#include "game_engine.hpp"
#include "hot_reload.hpp"
#include "objects/cubemap.hpp"
#include "objects/light_source.hpp"
#include "objects/objects.hpp"
#include "resource_loader.hpp"

#include <iostream>

class Experimental
{
public:
	Experimental(GameEngine& engine_) : engine(engine_)
	{
		// engine.draw_object(cube);
	}

	// manually triggered
	void process()
	{
		std::cout << "Experimental\n";
		auto chess_set = engine.resource_loader.load_objects(
			"../resources/models/chess.obj", std::vector<std::string_view>(33, "../resources/textures/chess.png"));
		for (auto& model : chess_set)
		{
			engine.spawn_object<Object>(std::move(model));
		}
	}

	// game engine triggers this periodically
	// time_delta is in seconds
	void process(float time_delta)
	{
		return; // uncomment to enable

		time_elapsed += time_delta;
		if (time_elapsed > 1.0f)
		{
			time_elapsed = 0;
		}
		
		// glm::vec3 axis;
		// HotReload::get().slfp->gen_vec(axis);
		// glm::quat rotator = glm::angleAxis(0.02f, glm::normalize(axis));
		// cube1.set_rotation(rotator * cube1.get_rotation());
	}

private:
	GameEngine& engine;
	float time_elapsed = 0;
	Cube cube;
};