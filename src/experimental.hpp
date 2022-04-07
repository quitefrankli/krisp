#pragma once

#include "game_engine.hpp"
#include "hot_reload.hpp"
#include "objects/objects.hpp"
#include "objects/light_source.hpp"

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
		
		auto& obj = engine.spawn_object<LightSource>(glm::vec3(1.0f, 1.0f, 1.0f));
		obj.set_position(glm::vec3(0.0f, 2.0f, 2.0f));
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