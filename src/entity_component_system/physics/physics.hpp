#pragma once

#include "identifications.hpp"
#include "physics_component.hpp"
#include "gravity.hpp"
#include "maths.hpp"


class ECS;

class PhysicsSystem : public GravitySystem
{
public:
	void add_physics_entity(const ObjectID id, const PhysicsComponent& physics_component)
	{
		physics_entities.emplace(id, physics_component);
	}

	void process(const float delta_secs);

	void remove_entity(const ObjectID id)
	{
		physics_entities.erase(id);
	}

protected:
	PhysicsComponent& get_physics_component(const ObjectID id) override
	{
		return physics_entities.at(id);
	}

private:
	std::unordered_map<ObjectID, PhysicsComponent> physics_entities;
};