#pragma once

#include "objects/objects.hpp"


class GameEngine;

class Experimental
{
public:
	Experimental(GameEngine& engine);

	void process(); // manually triggered
	void process(float time_delta); // game engine triggers this periodically

	Arrow v1;
	Arrow v2;
	Arrow v3;
	Arrow v4;
	Cube cube1;
	Cube cube2;

	GameEngine& engine;
};