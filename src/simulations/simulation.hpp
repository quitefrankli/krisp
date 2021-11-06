#pragma once

class GameEngine;

class Simulation
{
public:
	Simulation(GameEngine& engine);
	virtual void start() = 0;

protected:
	GameEngine& engine;
};