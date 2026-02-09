#pragma once

#include "physics_component.hpp"
#include "identifications.hpp"

#include <unordered_map>


class ForceSystem
{
public:
	virtual void compute_forces(const float delta_secs, 
								std::unordered_map<ObjectID, PhysicsComponent>& physics_entities) = 0;
};