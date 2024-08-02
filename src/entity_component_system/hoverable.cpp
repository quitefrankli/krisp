#include "hoverable.hpp"
#include "ecs.hpp"
#include "collision/collision_detector.hpp"
#include "collision/collider.hpp"
#include "utility.hpp"

#include <quill/LogMacros.h>
#include <fmt/core.h>


void HoverableSystem::add_hoverable_entity(EntityID id)
{
	if (!get_ecs().get_collider(id))
	{
		LOG_WARNING(Utility::get_logger(), "HoverableSystem: Added Entity {} with no collider", id.get_underlying());
		fmt::print("HoverableSystem: Warning added Entity {} with no collider\n", id.get_underlying());		
	}
	hoverable_entities.insert(id);
}

DetectedEntityCollision HoverableSystem::check_any_entity_hovered(const Maths::Ray& ray) const
{
	DetectedEntityCollision result;

	// TODO: implement functionality to check for "line_of_sight"
	float closest_clickable_distance = std::numeric_limits<float>::infinity();
	std::optional<Entity> closest_clickable;
	glm::vec3 closest_intersection;

	RayCollider ray_collider(ray);
	for (auto entity : hoverable_entities)
	{
		const auto* collider = get_ecs().get_collider(entity);
		if (!collider)
		{
			continue;
		}

		const auto collision_result = CollisionDetector::check_collision(&ray_collider, collider);
		if (!collision_result.bCollided)
		{
			continue;
		}

		auto distance = glm::distance2(ray.origin, collision_result.intersection);

		if (distance < closest_clickable_distance)
		{
			closest_clickable_distance = distance;
			closest_clickable = entity;
			closest_intersection = collision_result.intersection;
		}
	}

	if (closest_clickable)
	{
		result.bCollided = true;
		result.id = *closest_clickable;
		result.intersection = closest_intersection;
	}

	return result;
}
