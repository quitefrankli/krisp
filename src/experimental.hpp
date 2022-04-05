#pragma once

#include "game_engine.hpp"
#include "hot_reload.hpp"
#include "objects/objects.hpp"

#include <iostream>

static int tmp = 0;
static uint64_t tmp2;
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
		
		engine.spawn_object<Arrow>();

		// if (tmp % 2 == 0) {
		// 	auto& sphere = engine.spawn_object<Cube>();
		// 	tmp2 = sphere.get_id();
		// } else {
		// 	engine.delete_object(tmp2);
		// }

		// tmp++;
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