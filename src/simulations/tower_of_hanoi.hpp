#pragma once

#include "simulation.hpp"
#include "objects.hpp"


class TowerOfHanoi : public Simulation
{
public:
	using donut_t = HollowCylinder;
	using pillar_t = Cylinder;

	TowerOfHanoi(GameEngine& engine);
	virtual void start() override;

	void move_donut(std::shared_ptr<donut_t>& donut, int vector);

private:
	std::vector<std::shared_ptr<donut_t>> donuts;
	std::vector<std::shared_ptr<pillar_t>> pillars;
};