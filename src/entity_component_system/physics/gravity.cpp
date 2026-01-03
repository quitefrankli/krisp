#include "gravity.hpp"
#include "physics.hpp"
#include "game_engine.hpp"


static constexpr float G_SIMPLE = 0.5f; // m/s^2
static constexpr float G_GENERIC = 0.5f;

void GravitySystem::process(const float delta_secs, std::unordered_map<ObjectID, PhysicsComponent>& physics_entities)
{
	if (gravity_type == GravityType::EARTH_LIKE)
	{
		process_simplified_gravity(delta_secs, physics_entities);
	} else if (gravity_type == GravityType::TRUE)
	{
		process_generic_gravity(delta_secs, physics_entities);
	}
}

void GravitySystem::process_simplified_gravity(const float delta_secs,
                                               std::unordered_map<ObjectID, PhysicsComponent>& physics_entities)
{
	for (auto& [id, physics_component] : physics_entities)
	{
		physics_component.velocity.y -= G_SIMPLE * delta_secs;
	}
}

void GravitySystem::process_generic_gravity(const float delta_secs,
                                            std::unordered_map<ObjectID, PhysicsComponent>& physics_entities)
{
	auto& ecs = get_ecs();
	for (auto& [id_a, physics_component_a] : physics_entities)
	{
		auto& object_a = ecs.get_object(id_a);
		auto pos_a = object_a.get_position();

		for (auto& [id_b, physics_component_b] : physics_entities)
		{
			if (id_a == id_b) continue;

			auto& object_b = ecs.get_object(id_b);
			auto pos_b = object_b.get_position();

			const glm::vec3 direction = pos_b - pos_a;
			const float softening = 0.1f; // to avoid singularities
			const float distance_sq = glm::dot(direction, direction) + softening * softening;
			if (distance_sq < 0.0001f) continue; // avoid singularity

			const float force_magnitude = G_GENERIC * (physics_component_a.mass * physics_component_b.mass) / distance_sq;

			physics_component_a._net_force += glm::normalize(direction) * force_magnitude;
		}
	}
}
