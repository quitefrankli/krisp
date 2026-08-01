#include <entity_component_system/ecs.hpp>
#include <serialization/serializer.hpp>
#include <serialization/resource_provenance.hpp>
#include "serialization_test_helper.hpp"

#include <gtest/gtest.h>

namespace
{
struct EquipmentFixture
{
	EquipmentFixture()
	{
		ecs.add_object(wearer);
		ecs.add_object(other_wearer);
		ecs.add_object(first_item);
		ecs.add_object(second_item);
		Bone hand;
		hand.name = "hand";
		hand.relative_transform.set_pos({ 2.0f, 0.0f, 0.0f });
		const auto skeleton = ecs.add_skeleton({ hand });
		auto wearer_mesh = Renderable::make_default(ecs);
		wearer_mesh.pipeline_render_type = ERenderType::SKINNED_COLOR;
		wearer_renderable = ecs.add_renderable(std::move(wearer_mesh), wearer.get_id(), skeleton);
		auto other_mesh = Renderable::make_default(ecs);
		other_mesh.pipeline_render_type = ERenderType::SKINNED_COLOR;
		other_wearer_renderable = ecs.add_renderable(
			std::move(other_mesh), other_wearer.get_id(), skeleton);
		register_sources(wearer_renderable, "wearer.glb");
		register_sources(other_wearer_renderable, "other.glb");
	}

	void register_sources(RenderableID id, std::string source)
	{
		const auto& renderable = ecs.get_renderable(id).renderable;
		ResourceProvenance::register_mesh(renderable.get_mesh_id(), { .source = source });
		for (const auto material : renderable.get_material_ids())
			ResourceProvenance::register_material(material, { .source = source });
	}

	EquipmentDefinition definition(EquipmentSlot slot = EquipmentSlot::MainHand, std::string bone = "hand") const
	{
		Maths::Transform grip;
		grip.set_pos({ 0.0f, 1.0f, 0.0f });
		return { slot, std::move(bone), grip };
	}

	ECS ecs;
	Object wearer;
	Object other_wearer;
	Object first_item;
	Object second_item;
	RenderableID wearer_renderable;
	RenderableID other_wearer_renderable;
};
}

TEST(EquipmentSystem, equips_queries_and_follows_bone_pose)
{
	EquipmentFixture fixture;
	ASSERT_TRUE(fixture.ecs.equip(fixture.wearer.get_id(), fixture.wearer_renderable,
		fixture.first_item.get_id(), fixture.definition()));
	EXPECT_EQ(fixture.ecs.equipped_item(fixture.wearer.get_id(), EquipmentSlot::MainHand), fixture.first_item.get_id());
	fixture.ecs.process(0.0f);
	EXPECT_EQ(
		fixture.ecs.get_position(fixture.first_item.get_id()),
		glm::vec3(2.0f, 1.0f, 0.0f));
}

TEST(EquipmentSystem, renderable_replacement_remaps_equipment_references)
{
	EquipmentFixture fixture;
	ASSERT_TRUE(fixture.ecs.equip(
		fixture.wearer.get_id(), fixture.wearer_renderable,
		fixture.first_item.get_id(), fixture.definition()));
	auto replacement =
		fixture.ecs.get_renderable(fixture.wearer_renderable).renderable;
	fixture.wearer_renderable = fixture.ecs.replace_renderable(
		fixture.wearer_renderable, std::move(replacement));
	fixture.ecs.process(0.0f);

	Serializer serializer;
	EXPECT_NO_THROW(serialize_ecs(fixture.ecs, serializer));
	EXPECT_EQ(
		fixture.ecs.get_position(fixture.first_item.get_id()),
		glm::vec3(2.0f, 1.0f, 0.0f));
	EXPECT_EQ(
		fixture.ecs.equipped_item(
			fixture.wearer.get_id(), EquipmentSlot::MainHand),
		fixture.first_item.get_id());
}

TEST(EquipmentSystem, replacement_and_moving_item_keep_one_global_location)
{
	EquipmentFixture fixture;
	ASSERT_TRUE(fixture.ecs.equip(fixture.wearer.get_id(), fixture.wearer_renderable,
		fixture.first_item.get_id(), fixture.definition()));
	ASSERT_TRUE(fixture.ecs.equip(fixture.wearer.get_id(), fixture.wearer_renderable,
		fixture.second_item.get_id(), fixture.definition()));
	EXPECT_EQ(fixture.ecs.equipped_item(fixture.wearer.get_id(), EquipmentSlot::MainHand), fixture.second_item.get_id());
	ASSERT_TRUE(fixture.ecs.equip(fixture.other_wearer.get_id(), fixture.other_wearer_renderable,
		fixture.second_item.get_id(),
		fixture.definition(EquipmentSlot::OffHand)));
	EXPECT_FALSE(fixture.ecs.equipped_item(fixture.wearer.get_id(), EquipmentSlot::MainHand));
	EXPECT_EQ(fixture.ecs.equipped_item(fixture.other_wearer.get_id(), EquipmentSlot::OffHand), fixture.second_item.get_id());
}

TEST(EquipmentSystem, failed_attachment_leaves_existing_equipment_unchanged)
{
	EquipmentFixture fixture;
	ASSERT_TRUE(fixture.ecs.equip(fixture.wearer.get_id(), fixture.wearer_renderable,
		fixture.first_item.get_id(), fixture.definition()));
	EXPECT_FALSE(fixture.ecs.equip(fixture.wearer.get_id(), fixture.wearer_renderable,
		fixture.second_item.get_id(), fixture.definition(EquipmentSlot::MainHand, "missing")));
	EXPECT_EQ(fixture.ecs.equipped_item(fixture.wearer.get_id(), EquipmentSlot::MainHand), fixture.first_item.get_id());
}

TEST(EquipmentSystem, unequip_and_entity_removal_clean_references)
{
	EquipmentFixture fixture;
	ASSERT_TRUE(fixture.ecs.equip(fixture.wearer.get_id(), fixture.wearer_renderable,
		fixture.first_item.get_id(), fixture.definition()));
	EXPECT_EQ(fixture.ecs.unequip(fixture.wearer.get_id(), EquipmentSlot::MainHand), fixture.first_item.get_id());
	EXPECT_FALSE(fixture.ecs.unequip(fixture.wearer.get_id(), EquipmentSlot::MainHand));
	ASSERT_TRUE(fixture.ecs.equip(fixture.wearer.get_id(), fixture.wearer_renderable,
		fixture.first_item.get_id(), fixture.definition()));
	fixture.ecs.remove_object(fixture.first_item.get_id());
	EXPECT_FALSE(fixture.ecs.equipped_item(fixture.wearer.get_id(), EquipmentSlot::MainHand));
	ASSERT_TRUE(fixture.ecs.equip(fixture.wearer.get_id(), fixture.wearer_renderable,
		fixture.second_item.get_id(), fixture.definition()));
	fixture.ecs.remove_object(fixture.wearer.get_id());
	EXPECT_FALSE(fixture.ecs.equipped_item(fixture.wearer.get_id(), EquipmentSlot::MainHand));
}

TEST(EquipmentSystem, serializes_and_restores_equipment)
{
	EquipmentFixture source;
	ASSERT_TRUE(source.ecs.equip(source.wearer.get_id(), source.wearer_renderable,
		source.first_item.get_id(), source.definition(EquipmentSlot::Head)));
	Serializer serializer;
	serialize_ecs(source.ecs, serializer);

	ASSERT_EQ(source.ecs.unequip(source.wearer.get_id(), EquipmentSlot::Head), source.first_item.get_id());
	deserialize_ecs(source.ecs, Deserializer::parse(serializer.emit()));
	EXPECT_EQ(source.ecs.equipped_item(source.wearer.get_id(), EquipmentSlot::Head), source.first_item.get_id());
	EXPECT_FALSE(source.ecs.equipped_item(source.wearer.get_id(), EquipmentSlot::MainHand));
}

TEST(EquipmentSystem, malformed_attachment_fails_without_replacing_current_equipment)
{
	EquipmentFixture fixture;
	ASSERT_TRUE(fixture.ecs.equip(
		fixture.wearer.get_id(), fixture.wearer_renderable,
		fixture.first_item.get_id(), fixture.definition()));
	Serializer serializer;
	fixture.ecs.EquipmentSystem::serialize(serializer);
	std::string malformed = serializer.emit();
	malformed.replace(malformed.find("attachment_bone: hand"), std::string("attachment_bone: hand").size(),
		"attachment_bone: missing");

	SceneResourceReader resources(fixture.ecs, serialization_test_resource_directory());
	resources.register_renderable_id(fixture.wearer_renderable, fixture.wearer_renderable);
	EXPECT_THROW(fixture.ecs.EquipmentSystem::deserialize(
		Deserializer::parse(malformed), resources), SerializationError);
	EXPECT_EQ(
		fixture.ecs.equipped_item(fixture.wearer.get_id(), EquipmentSlot::MainHand),
		fixture.first_item.get_id());
}

TEST(EquipmentSystem, checkpoint_without_equipment_clears_current_equipment)
{
	EquipmentFixture fixture;
	ASSERT_TRUE(fixture.ecs.equip(
		fixture.wearer.get_id(), fixture.wearer_renderable,
		fixture.first_item.get_id(), fixture.definition()));
	Serializer empty;

	SceneResourceReader resources(fixture.ecs, serialization_test_resource_directory());
	fixture.ecs.EquipmentSystem::deserialize(Deserializer::parse(empty.emit()), resources);

	EXPECT_FALSE(fixture.ecs.equipped_item(
		fixture.wearer.get_id(), EquipmentSlot::MainHand));
}
