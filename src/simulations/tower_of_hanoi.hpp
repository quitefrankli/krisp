#pragma once

#include "simulation.hpp"
#include "objects.hpp"


class TowerOfHanoi : public Simulation
{
public:
	TowerOfHanoi(GameEngine& engine);
	virtual void start() override;

private:
	std::vector<std::shared_ptr<Object>> donuts;
	std::vector<std::shared_ptr<Object>> pillars;
};