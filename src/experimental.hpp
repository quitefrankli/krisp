#pragma once

#include "game_engine.hpp"
#include "objects/objects.hpp"

#include <iostream>


class Experimental
{
public:
	Experimental(GameEngine& engine) : 
		engine(engine)
	{
	}

	// manually triggered
	void process()
	{
	}

	// game engine triggers this periodically
	// time_delta is in seconds
	void process(float time_delta)
	{
		time_elapsed += time_delta;
		
		if (underlying_entity_id == -1)
		{
			return;
		}
	}

private:
	GameEngine& engine;
	float time_elapsed = 0;
	int underlying_entity_id = -1;
};