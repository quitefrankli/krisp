#include "clickable.hpp"
#include "ecs.hpp"
#include "utility.hpp"
#include "serialization/serializer.hpp"

#include <quill/LogMacros.h>
#include <fmt/core.h>

#include <algorithm>
#include <stdexcept>
#include <vector>

void ClickableSystem::add_clickable_entity(EntityID id)
{
	if (!get_ecs().has_object(id))
		throw std::invalid_argument(fmt::format(
			"ClickableSystem: Entity {} is not registered with the ECS",
			id.get_underlying()));
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
		if (!get_ecs().has_object(id)) {
			throw SerializationError("Clickable entity references missing object at "
				"$.clickable_system[" + std::to_string(index) + "].entity_id");
		}
		if (!restored_entities.insert(id).second) {
			throw SerializationError("Duplicate clickable entity at $.clickable_system["
				+ std::to_string(index) + "].entity_id");
		}
	}
	clickable_entities = std::move(restored_entities);
}

DetectedEntityCollision ClickableSystem::check_any_entity_clicked(const Maths::Ray& ray) const
{
	const std::vector<EntityID> candidates(
		clickable_entities.begin(), clickable_entities.end());
	return get_ecs().raycast_entities(ray, candidates);
}
