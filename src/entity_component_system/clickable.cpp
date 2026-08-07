#include "clickable.hpp"
#include "ecs.hpp"
#include "collision/collision_detector.hpp"
#include "collision/collider.hpp"
#include "utility.hpp"
#include "serialization/serializer.hpp"

#include <quill/LogMacros.h>
#include <fmt/core.h>
#include <glm/gtx/norm.hpp>

#include <algorithm>
#include <vector>

void ClickableSystem::add_clickable_entity(EntityID id)
{
	if (!get_ecs().has_rigid_body(id) && !get_ecs().has_collider(id))
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
	std::vector<EntityID> physics_candidates;
	std::vector<EntityID> collider_candidates;
	physics_candidates.reserve(clickable_entities.size());
	collider_candidates.reserve(clickable_entities.size());
	for (const EntityID id : clickable_entities)
	{
		if (get_ecs().has_rigid_body(id))
			physics_candidates.push_back(id);
		else if (get_ecs().has_collider(id))
			collider_candidates.push_back(id);
	}

	const auto physics_hit = get_ecs().PhysicsSystem::raycast(ray, physics_candidates);
	const auto collider_hit = get_ecs().ColliderSystem::raycast(ray, collider_candidates);
	if (!physics_hit.bCollided)
		return collider_hit;
	if (!collider_hit.bCollided)
		return physics_hit;
	return glm::distance2(ray.origin, physics_hit.intersection)
		<= glm::distance2(ray.origin, collider_hit.intersection)
		? physics_hit : collider_hit;
}
