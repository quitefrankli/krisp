#pragma once


class GameEngine;

class Experimental
{
public:
	Experimental(GameEngine& engine);

	void process(); // manually triggered
	void process(float time_delta); // game engine triggers this periodically

private:
	GameEngine& engine;
};