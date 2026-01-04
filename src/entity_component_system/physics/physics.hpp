#pragma once

#include "identifications.hpp"
#include "physics_component.hpp"
#include "force_system.hpp"
#include "gravity.hpp"
#include "maths.hpp"

#include <memory>


class ECS;

class PhysicsSystem
{
public:
	PhysicsSystem();

	void add_physics_entity(const ObjectID id, const PhysicsComponent& physics_component)
	{
		physics_entities.emplace(id, physics_component);
	}

	void process(const float delta_secs);

	void remove_entity(const ObjectID id)
	{
		physics_entities.erase(id);
	}

	void add_force_system(std::unique_ptr<ForceSystem> force_system)
	{
		force_systems.push_back(std::move(force_system));
	}

	GravitySystem& get_gravity_system();

protected:
	virtual ECS& get_ecs() = 0;

private:
	void prepare_components();

private:
	std::unordered_map<ObjectID, PhysicsComponent> physics_entities;
	std::vector<std::unique_ptr<ForceSystem>> force_systems;
};