#include "test_helper.hpp"

#include <entity_component_system/transformation_system.hpp>
#include <serialization/serializer.hpp>
#include <serialization/serialization_helpers.hpp>

#include <gtest/gtest.h>

#include <type_traits>


static_assert(!std::is_copy_constructible_v<TransformationComponent>);
static_assert(!std::is_copy_assignable_v<TransformationComponent>);
static_assert(!std::is_move_constructible_v<TransformationComponent>);
static_assert(!std::is_move_assignable_v<TransformationComponent>);


TEST(TransformationSystemTests, defaults_to_identity_and_updates_world_components)
{
	TransformationSystem transformations;
	const EntityID entity(1);
	transformations.add_transformation(entity);

	EXPECT_TRUE(glm_equal(transformations.get_position(entity), Maths::zero_vec));
	EXPECT_TRUE(glm_equal(transformations.get_rotation(entity), Maths::identity_quat));
	EXPECT_TRUE(glm_equal(transformations.get_scale(entity), Maths::identity_vec));

	const auto rotation = glm::angleAxis(Maths::PI / 2.0f, Maths::up_vec);
	transformations.set_position(entity, { 1.0f, 2.0f, 3.0f });
	transformations.set_rotation(entity, rotation);
	transformations.set_scale(entity, { 2.0f, 3.0f, 4.0f });

	EXPECT_TRUE(glm_equal(transformations.get_position(entity), glm::vec3(1.0f, 2.0f, 3.0f)));
	EXPECT_TRUE(glm_equal(transformations.get_rotation(entity), rotation));
	EXPECT_TRUE(glm_equal(transformations.get_scale(entity), glm::vec3(2.0f, 3.0f, 4.0f)));
	EXPECT_TRUE(glm_equal(
		transformations.get_relative_transform(entity),
		transformations.get_transform(entity)));
}

TEST(TransformationSystemTests, stored_component_routes_world_and_relative_operations)
{
	TransformationSystem transformations;
	const EntityID parent(1);
	const EntityID child(2);
	transformations.add_transformation(parent);
	transformations.add_transformation(child);

	auto& parent_transform = transformations.get_transformation(parent);
	auto& child_transform = transformations.get_transformation(child);
	EXPECT_EQ(&parent_transform, &transformations.get_transformation(parent));
	EXPECT_EQ(&child_transform, &transformations.get_transformation(child));
	parent_transform.set_position({ 2.0f, 0.0f, 0.0f });
	child_transform.set_position({ 3.0f, 0.0f, 0.0f });
	ASSERT_TRUE(child_transform.attach_to(parent_transform));
	child_transform.set_relative_position({ 0.0f, 4.0f, 0.0f });

	EXPECT_TRUE(glm_equal(child_transform.get_position(), glm::vec3(2.0f, 4.0f, 0.0f)));
	EXPECT_EQ(child_transform.get_parent_id(), parent);

	const TransformationSystem& read_only = transformations;
	const auto& const_child = read_only.get_transformation(child);
	EXPECT_TRUE(glm_equal(const_child.get_relative_position(), glm::vec3(0.0f, 4.0f, 0.0f)));
	EXPECT_EQ(const_child.get_entity_id(), child);
}

TEST(TransformationSystemTests, propagates_parent_changes_through_multiple_levels)
{
	TransformationSystem transformations;
	const EntityID root(1);
	const EntityID child(2);
	const EntityID grandchild(3);
	transformations.add_transformation(root);
	transformations.add_transformation(child);
	transformations.add_transformation(grandchild);
	transformations.set_position(child, { 1.0f, 0.0f, 0.0f });
	transformations.set_position(grandchild, { 1.0f, 1.0f, 0.0f });

	ASSERT_TRUE(transformations.attach_to(child, root));
	ASSERT_TRUE(transformations.attach_to(grandchild, child));
	transformations.set_position(root, { 2.0f, 0.0f, 0.0f });

	EXPECT_TRUE(glm_equal(transformations.get_position(child), glm::vec3(3.0f, 0.0f, 0.0f)));
	EXPECT_TRUE(glm_equal(transformations.get_position(grandchild), glm::vec3(3.0f, 1.0f, 0.0f)));

	const auto rotation = glm::angleAxis(Maths::PI / 2.0f, Maths::up_vec);
	transformations.set_rotation(root, rotation);
	EXPECT_TRUE(glm_equal(transformations.get_position(child), glm::vec3(2.0f, 0.0f, -1.0f)));
	EXPECT_TRUE(glm_equal(transformations.get_position(grandchild), glm::vec3(2.0f, 1.0f, -1.0f)));
}

TEST(TransformationSystemTests, attaching_detaching_and_removal_preserve_world_pose)
{
	TransformationSystem transformations;
	const EntityID parent(1);
	const EntityID child(2);
	const EntityID grandchild(3);
	for (const auto id : { parent, child, grandchild })
		transformations.add_transformation(id);
	transformations.set_position(parent, { 4.0f, 0.0f, 0.0f });
	transformations.set_position(child, { 1.0f, 2.0f, 3.0f });
	transformations.set_position(grandchild, { 2.0f, 3.0f, 4.0f });

	ASSERT_TRUE(transformations.attach_to(child, parent));
	ASSERT_TRUE(transformations.attach_to(grandchild, child));
	EXPECT_TRUE(glm_equal(transformations.get_position(child), glm::vec3(1.0f, 2.0f, 3.0f)));
	EXPECT_TRUE(glm_equal(transformations.get_position(grandchild), glm::vec3(2.0f, 3.0f, 4.0f)));

	transformations.remove_transformation(child);
	EXPECT_FALSE(transformations.has_transformation(child));
	EXPECT_EQ(transformations.get_parent_id(grandchild), std::nullopt);
	EXPECT_TRUE(glm_equal(transformations.get_position(grandchild), glm::vec3(2.0f, 3.0f, 4.0f)));
}

TEST(TransformationSystemTests, rejects_self_and_descendant_cycles_without_mutation)
{
	TransformationSystem transformations;
	const EntityID root(1);
	const EntityID child(2);
	const EntityID grandchild(3);
	for (const auto id : { root, child, grandchild })
		transformations.add_transformation(id);
	ASSERT_TRUE(transformations.attach_to(child, root));
	ASSERT_TRUE(transformations.attach_to(grandchild, child));

	EXPECT_FALSE(transformations.attach_to(root, root));
	EXPECT_FALSE(transformations.attach_to(root, grandchild));
	EXPECT_EQ(transformations.get_parent_id(root), std::nullopt);
	EXPECT_EQ(transformations.get_parent_id(child), root);
	EXPECT_EQ(transformations.get_parent_id(grandchild), child);
}

TEST(TransformationSystemSerialization, round_trips_persistent_hierarchy_and_retains_transients)
{
	TransformationSystem source;
	const EntityID parent(1);
	const EntityID child(2);
	const EntityID transient(3);
	source.add_transformation(parent);
	source.add_transformation(child);
	source.add_transformation(transient, TransformationPersistence::Transient);
	source.set_position(parent, { 4.0f, 5.0f, 6.0f });
	source.set_position(child, { 7.0f, 8.0f, 9.0f });
	source.set_position(transient, { 10.0f, 11.0f, 12.0f });
	ASSERT_TRUE(source.attach_to(child, parent));

	Serializer serializer;
	source.serialize(serializer);

	TransformationSystem restored;
	restored.add_transformation(transient, TransformationPersistence::Transient);
	restored.set_position(transient, { -1.0f, -2.0f, -3.0f });
	restored.deserialize(Deserializer::parse(serializer.emit()));

	EXPECT_EQ(restored.get_parent_id(child), parent);
	EXPECT_TRUE(glm_equal(restored.get_position(parent), glm::vec3(4.0f, 5.0f, 6.0f)));
	EXPECT_TRUE(glm_equal(restored.get_position(child), glm::vec3(7.0f, 8.0f, 9.0f)));
	EXPECT_TRUE(glm_equal(restored.get_position(transient), glm::vec3(-1.0f, -2.0f, -3.0f)));
}

TEST(TransformationSystemSerialization, malformed_hierarchy_fails_atomically)
{
	TransformationSystem transformations;
	const EntityID existing(9);
	transformations.add_transformation(existing);
	transformations.set_position(existing, { 1.0f, 2.0f, 3.0f });

	Serializer malformed;
	auto entry = malformed.sequence("transformation_system").append_map();
	entry.write("entity_id", 1);
	entry.write("parent_id", 2);
	Serialization::write_transform(entry, "local_transform", Maths::Transform{});

	EXPECT_THROW(
		transformations.deserialize(Deserializer::parse(malformed.emit())),
		SerializationError);
	EXPECT_TRUE(transformations.has_transformation(existing));
	EXPECT_TRUE(glm_equal(transformations.get_position(existing), glm::vec3(1.0f, 2.0f, 3.0f)));
}
