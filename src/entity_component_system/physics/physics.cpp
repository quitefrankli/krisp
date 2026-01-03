#include "physics.hpp"
#include "entity_component_system/ecs.hpp"


void PhysicsSystem::process(const float delta_secs) 
{
	//
	// With Verlet Integration
	//

	auto& ecs = get_ecs();

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

	// zero out forces...
	for (auto& [id, physics_comp] : physics_entities)
	{
		physics_comp._net_force = Maths::zero_vec;
	}

	// accumulate forces...
	GravitySystem::process(delta_secs, physics_entities);

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
