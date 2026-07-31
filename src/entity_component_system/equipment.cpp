#include "equipment.hpp"
#include "ecs.hpp"
#include "serialization/serialization_helpers.hpp"

#include <algorithm>
#include <ranges>

namespace
{
constexpr size_t slot_index(const EquipmentSlot slot)
{
	return static_cast<size_t>(slot);
}

bool valid_slot(const std::uint64_t value)
{
	return value <= static_cast<std::uint64_t>(EquipmentSlot::Chest);
}
}

bool EquipmentSystem::equip(
	const Entity wearer, const RenderableID source_renderable, const Entity item,
	const EquipmentDefinition& definition)
{
	if (!valid_slot(static_cast<std::uint64_t>(definition.slot)))
		return false;
	if (!get_ecs().has_renderable(source_renderable)
		|| get_ecs().get_renderable(source_renderable).object_id != wearer)
		return false;
	if (!get_ecs().attach_entity_to_bone(
		item, source_renderable, definition.attachment_bone, definition.grip_transform))
		return false;

	auto& slots = equipment[wearer];
	auto& destination = slots[slot_index(definition.slot)];
	const auto old_item = destination ? std::optional<Entity>(destination->item) : std::nullopt;
	if (const auto location = item_locations.find(item); location != item_locations.end())
	{
		auto& old_slots = equipment.at(location->second.first);
		old_slots[slot_index(location->second.second)].reset();
		if (location->second.first != wearer && std::ranges::all_of(old_slots, [](const auto& value) { return !value; }))
			equipment.erase(location->second.first);
	}
	destination = EquippedItem{ item, source_renderable, definition };
	item_locations.insert_or_assign(item, std::pair{ wearer, definition.slot });
	if (old_item && *old_item != item)
	{
		item_locations.erase(*old_item);
		get_ecs().detach_entity_from_bone(*old_item);
	}
	return true;
}

std::optional<Entity> EquipmentSystem::unequip(const Entity wearer, const EquipmentSlot slot)
{
	if (!valid_slot(static_cast<std::uint64_t>(slot)))
		return std::nullopt;
	const auto wearer_it = equipment.find(wearer);
	if (wearer_it == equipment.end())
		return std::nullopt;
	auto& equipped = wearer_it->second[slot_index(slot)];
	if (!equipped)
		return std::nullopt;
	const Entity item = equipped->item;
	get_ecs().detach_entity_from_bone(item);
	equipped.reset();
	item_locations.erase(item);
	if (std::ranges::all_of(wearer_it->second, [](const auto& value) { return !value; }))
		equipment.erase(wearer_it);
	return item;
}

std::optional<Entity> EquipmentSystem::equipped_item(const Entity wearer, const EquipmentSlot slot) const
{
	if (!valid_slot(static_cast<std::uint64_t>(slot)))
		return std::nullopt;
	const auto wearer_it = equipment.find(wearer);
	if (wearer_it == equipment.end())
		return std::nullopt;
	const auto& equipped = wearer_it->second[slot_index(slot)];
	return equipped ? std::optional<Entity>(equipped->item) : std::nullopt;
}

void EquipmentSystem::remove_entity(const Entity id)
{
	if (const auto wearer = equipment.find(id); wearer != equipment.end())
	{
		for (const auto& equipped : wearer->second)
			if (equipped) {
				get_ecs().detach_entity_from_bone(equipped->item);
				item_locations.erase(equipped->item);
			}
		equipment.erase(wearer);
	}
	if (const auto location = item_locations.find(id); location != item_locations.end())
	{
		auto& slots = equipment.at(location->second.first);
		slots[slot_index(location->second.second)].reset();
		item_locations.erase(location);
		if (std::ranges::all_of(slots, [](const auto& value) { return !value; }))
			equipment.erase(location->second.first);
	}
	get_ecs().detach_entity_from_bone(id);
}

void EquipmentSystem::on_renderable_removed(const RenderableID id)
{
	std::vector<std::pair<Entity, EquipmentSlot>> removals;
	for (const auto& [wearer, slots] : equipment)
		for (std::size_t index = 0; index < slots.size(); ++index)
			if (slots[index] && slots[index]->source_renderable == id)
				removals.emplace_back(wearer, static_cast<EquipmentSlot>(index));
	for (const auto& [wearer, slot] : removals)
		unequip(wearer, slot);
}

void EquipmentSystem::on_renderable_replaced(
	const RenderableID old_id, const RenderableID new_id)
{
	for (auto& [_, slots] : equipment)
		for (auto& equipped : slots)
			if (equipped && equipped->source_renderable == old_id)
				equipped->source_renderable = new_id;
}

void EquipmentSystem::serialize(Serializer& out) const
{
	auto entries = out.sequence("equipment_system");
	std::vector<Entity> wearers;
	for (const auto& [wearer, _] : equipment)
		wearers.push_back(wearer);
	std::ranges::sort(wearers);
	for (const auto wearer : wearers)
		for (size_t index = 0; index < equipment.at(wearer).size(); ++index)
			if (const auto& equipped = equipment.at(wearer)[index]) {
				const auto& source = get_ecs().get_renderable(equipped->source_renderable);
				if (get_ecs().is_transient_transformation(wearer)
					|| get_ecs().is_transient_transformation(equipped->item)
					|| (source.object_id
						&& get_ecs().is_transient_transformation(*source.object_id)))
				{
					continue;
				}
				auto entry = entries.append_map();
				entry.write("wearer_id", wearer.get_underlying());
				entry.write("item_id", equipped->item.get_underlying());
				entry.write("source_renderable_id", equipped->source_renderable.get_underlying());
				entry.write("slot", index);
				entry.write("attachment_bone", equipped->definition.attachment_bone);
				Serialization::write_transform(entry, "grip_transform", equipped->definition.grip_transform);
			}
}

void EquipmentSystem::deserialize(const Deserializer& in)
{
	const auto keys = in.keys();
	if (std::ranges::find(keys, "equipment_system") == keys.end()) {
		for (const auto& [item, _] : item_locations)
			get_ecs().detach_entity_from_bone(item);
		equipment.clear();
		item_locations.clear();
		return;
	}
	std::unordered_map<Entity, Slots> restored;
	std::unordered_map<Entity, std::pair<Entity, EquipmentSlot>> restored_locations;
	const auto entries = in.child("equipment_system").elements();
	for (size_t index = 0; index < entries.size(); ++index)
	{
		const auto& entry = entries[index];
		const auto slot_value = entry.read<std::uint64_t>("slot");
		if (!valid_slot(slot_value))
			throw SerializationError("Invalid equipment slot at $.equipment_system[" + std::to_string(index) + "].slot");
		const Entity wearer(entry.read<std::uint64_t>("wearer_id"));
		const Entity item(entry.read<std::uint64_t>("item_id"));
		const auto slot = static_cast<EquipmentSlot>(slot_value);
		auto& destination = restored[wearer][slot_index(slot)];
		if (destination || !restored_locations.emplace(item, std::pair{ wearer, slot }).second)
			throw SerializationError("Duplicate equipment entry at $.equipment_system[" + std::to_string(index) + "]");
		const RenderableID source_renderable(entry.read<std::uint64_t>("source_renderable_id"));
		if (!get_ecs().has_renderable(source_renderable)
			|| get_ecs().get_renderable(source_renderable).object_id != wearer)
			throw SerializationError("Invalid equipment source renderable at " + entry.path());
		destination = EquippedItem{ item, source_renderable,
			{ slot, entry.read<std::string>("attachment_bone"),
			Serialization::read_transform(entry, "grip_transform") } };
	}

	std::vector<EquippedItem> attached;
	try {
		for (const auto& [wearer, slots] : restored)
			for (const auto& equipped : slots)
				if (equipped) {
					if (!get_ecs().attach_entity_to_bone(equipped->item, equipped->source_renderable,
						equipped->definition.attachment_bone, equipped->definition.grip_transform))
						throw SerializationError("Invalid equipment attachment");
					attached.push_back(*equipped);
				}
	} catch (...) {
		for (const auto& equipped : attached)
			get_ecs().detach_entity_from_bone(equipped.item);
		for (const auto& [wearer, slots] : equipment)
			for (const auto& equipped : slots)
				if (equipped)
					get_ecs().attach_entity_to_bone(equipped->item, equipped->source_renderable,
						equipped->definition.attachment_bone, equipped->definition.grip_transform);
		throw;
	}
	for (const auto& [item, _] : item_locations)
		if (!restored_locations.contains(item))
			get_ecs().detach_entity_from_bone(item);
	equipment = std::move(restored);
	item_locations = std::move(restored_locations);
}
