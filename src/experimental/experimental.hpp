#pragma once

#include "objects/objects.hpp"


class GameEngine;

class Experimental
{
public:
	Experimental(GameEngine& engine);

	void process();

	Arrow v1;
	Arrow v2;
	Arrow v3;
	Arrow v4;

	GameEngine& engine;
};