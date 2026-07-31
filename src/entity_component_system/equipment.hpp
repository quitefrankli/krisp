#pragma once

#include "skeletal.hpp"

#include <array>
#include <optional>
#include <string>
#include <unordered_map>

enum class EquipmentSlot
{
	MainHand,
	OffHand,
	Head,
	Chest,
};

struct EquipmentDefinition
{
	EquipmentSlot slot = EquipmentSlot::MainHand;
	std::string attachment_bone;
	Maths::Transform grip_transform;
};

class ECS;
class Serializer;
class Deserializer;

class EquipmentSystem
{
public:
	virtual ECS& get_ecs() = 0;
	virtual const ECS& get_ecs() const = 0;

	bool equip(Entity wearer, RenderableID source_renderable, Entity item,
		const EquipmentDefinition& definition);
	std::optional<Entity> unequip(Entity wearer, EquipmentSlot slot);
	std::optional<Entity> equipped_item(Entity wearer, EquipmentSlot slot) const;
	void serialize(Serializer& out) const;
	void deserialize(const Deserializer& in);

protected:
	void remove_entity(Entity id);
	void on_renderable_removed(RenderableID id);
	friend class RenderableSystem;

private:
	static constexpr size_t slot_count = 4;

	struct EquippedItem
	{
		Entity item;
		RenderableID source_renderable;
		EquipmentDefinition definition;
	};

	using Slots = std::array<std::optional<EquippedItem>, slot_count>;
	std::unordered_map<Entity, Slots> equipment;
	std::unordered_map<Entity, std::pair<Entity, EquipmentSlot>> item_locations;
};
