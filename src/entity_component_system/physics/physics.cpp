#include "physics.hpp"
#include "entity_component_system/ecs.hpp"


PhysicsSystem::PhysicsSystem()
{
	force_systems.push_back(std::make_unique<GravitySystem>());
}

void PhysicsSystem::process(const float delta_secs)
{
	static auto& ecs = get_ecs();
	prepare_components();

	//
	// With Verlet Integration
	//

	//
	// 1. advance position
	//
	for (auto& [id, physics_comp] : physics_entities)
	{
		auto& object = ecs.get_object(id);
		auto pos = object.get_position();
		pos += physics_comp.velocity * delta_secs + 0.5f * physics_comp.acceleration * delta_secs * delta_secs;
		object.set_position(pos);
	}

	//
	// 2. recompute forces at new position
	//
	for (auto& force_system : force_systems)
	{
		force_system->compute_forces(delta_secs, physics_entities);
	}

	//
	// 3. apply forces and update velocity
	//
	for (auto& [id, physics_comp] : physics_entities)
	{
		auto& object = ecs.get_object(id);
		const glm::vec3 new_acceleration = physics_comp._net_force / physics_comp.mass;
		physics_comp.velocity += 0.5f * (physics_comp.acceleration + new_acceleration) * delta_secs;
	}
}

GravitySystem& PhysicsSystem::get_gravity_system()
{
	return static_cast<GravitySystem&>(*force_systems[0]);
}

void PhysicsSystem::prepare_components()
{
	static auto& ecs = get_ecs();
	for (auto& [id, physics_comp] : physics_entities)
	{
		physics_comp.position = ecs.get_object(id).get_position();
		physics_comp._net_force = Maths::zero_vec;
	}
}
