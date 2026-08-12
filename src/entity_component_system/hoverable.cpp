#include "hoverable.hpp"
#include "ecs.hpp"
#include "utility.hpp"
#include "serialization/serializer.hpp"

#include <quill/LogMacros.h>
#include <fmt/core.h>

#include <algorithm>
#include <stdexcept>
#include <vector>

void HoverableSystem::add_hoverable_entity(EntityID id)
{
	if (!get_ecs().has_object(id))
		throw std::invalid_argument(fmt::format(
			"HoverableSystem: Entity {} is not registered with the ECS",
			id.get_underlying()));
	if (!get_ecs().has_rigid_body(id) && !get_ecs().has_collider(id))
	{
		LOG_WARNING(Utility::get_logger(), "HoverableSystem: Added Entity {} with no collider", id.get_underlying());
	}
	hoverable_entities.insert(id);
}

void HoverableSystem::serialize(Serializer& out) const
{
	std::vector<std::uint64_t> entity_ids;
	entity_ids.reserve(hoverable_entities.size());
	for (const auto id : hoverable_entities)
		entity_ids.push_back(id.get_underlying());
	std::ranges::sort(entity_ids);
	auto entries = out.sequence("hoverable_system");
	for (const auto id : entity_ids)
		entries.append_map().write("entity_id", id);
}

void HoverableSystem::deserialize(const Deserializer& in)
{
	std::unordered_set<EntityID> restored;
	const auto entries = in.child("hoverable_system").elements();
	for (std::size_t index = 0; index < entries.size(); ++index) {
		const EntityID id(entries[index].read<std::uint64_t>("entity_id"));
		if (!get_ecs().has_object(id)) {
			throw SerializationError("Hoverable entity references missing object at "
				"$.hoverable_system[" + std::to_string(index) + "].entity_id");
		}
		if (!restored.insert(id).second) {
			throw SerializationError("Duplicate hoverable entity at $.hoverable_system["
				+ std::to_string(index) + "].entity_id");
		}
	}
	hoverable_entities = std::move(restored);
}

DetectedEntityCollision HoverableSystem::check_any_entity_hovered(const Maths::Ray& ray) const
{
	std::vector<EntityID> candidates(hoverable_entities.begin(), hoverable_entities.end());
	return get_ecs().raycast_entities(ray, candidates);
}
