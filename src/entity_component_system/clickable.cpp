#include "clickable.hpp"
#include "ecs.hpp"
#include "collision/collision_detector.hpp"
#include "collision/collider.hpp"
#include "utility.hpp"
#include "serialization/serializer.hpp"

#include <quill/LogMacros.h>
#include <fmt/core.h>

#include <algorithm>
#include <vector>

void ClickableSystem::add_clickable_entity(EntityID id)
{
	if (!get_ecs().get_collider(id))
	{
		LOG_WARNING(Utility::get_logger(), "ClickableSystem: Added Entity {} with no collider", id.get_underlying());
	}
	clickable_entities.insert(id);
}

void ClickableSystem::serialize(Serializer& out) const
{
	std::vector<std::uint64_t> entity_ids;
	entity_ids.reserve(clickable_entities.size());
	for (const auto id : clickable_entities)
		entity_ids.push_back(id.get_underlying());
	std::ranges::sort(entity_ids);

	auto entries = out.sequence("clickable_system");
	for (const auto id : entity_ids)
		entries.append_map().write("entity_id", id);
}

void ClickableSystem::deserialize(const Deserializer& in)
{
	std::unordered_set<EntityID> restored_entities;
	const auto entries = in.child("clickable_system").elements();
	for (std::size_t index = 0; index < entries.size(); ++index) {
		const EntityID id(entries[index].read<std::uint64_t>("entity_id"));
		if (!restored_entities.insert(id).second) {
			throw SerializationError("Duplicate clickable entity at $.clickable_system["
				+ std::to_string(index) + "].entity_id");
		}
	}
	clickable_entities = std::move(restored_entities);
}

DetectedEntityCollision ClickableSystem::check_any_entity_clicked(const Maths::Ray& ray) const
{
	DetectedEntityCollision result;

	// TODO: implement functionality to check for "line_of_sight"
	float closest_clickable_distance = std::numeric_limits<float>::infinity();
	std::optional<Entity> closest_clickable;
	glm::vec3 closest_intersection;

	RayCollider ray_collider(ray);
	for (auto entity : clickable_entities)
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
