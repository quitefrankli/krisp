#include "gravity.hpp"
#include "physics.hpp"
#include "game_engine.hpp"


static constexpr float G_SIMPLE = 0.5f; // m/s^2
static constexpr float G_GENERIC = 0.5f;

void GravitySystem::compute_forces(const float delta_secs,
	std::unordered_map<ObjectID, PhysicsComponent>& physics_entities)
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
	for (auto& [id_a, a] : physics_entities)
	{
		for (auto& [id_b, b] : physics_entities)
		{
			if (id_a == id_b) continue;

			const glm::vec3 direction = b.position - a.position;
			const float softening = 0.01f; // to avoid singularities
			const float distance_sq = glm::dot(direction, direction) + softening;

			const float force_magnitude = G_GENERIC * (a.mass * b.mass) / distance_sq;

			a._net_force += glm::normalize(direction) * force_magnitude;
		}
	}
}
