#pragma once

class GameEngine;

class Simulation
{
public:
	Simulation(GameEngine& engine);

protected:
	GameEngine& engine;
};