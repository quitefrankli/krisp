#include "test_helper.hpp"

#include <entity_component_system/ecs.hpp>
#include <serialization/serializer.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <vector>
#include <unordered_set>


class ClickableECSFixture : public testing::Test
{
public:
	ClickableECSFixture()
	{
		ecs.add_object(object1);
		ecs.add_object(object2);
		
		ecs.set_position(object2.get_id(), glm::vec3(1.0f, 1.0f, 1.0f));

		ecs.add_rigid_body(object1.get_id(), RigidBodyDefinition{ .shape = SpherePhysicsShape{0.5f} });
		ecs.add_rigid_body(object2.get_id(), RigidBodyDefinition{ .shape = SpherePhysicsShape{0.5f} });

		ecs.add_clickable_entity(object1.get_id());
		ecs.add_clickable_entity(object2.get_id());
	}

	ECS ecs;
	Object object1;
	Object object2;
};

TEST_F(ClickableECSFixture, hit_obj1)
{
	Maths::Ray ray{-Maths::forward_vec, Maths::forward_vec};
	auto res = ecs.check_any_entity_clicked(ray);
	ASSERT_TRUE(res.bCollided);
	ASSERT_EQ(res.id, object1.get_id());
	ASSERT_TRUE(glm_equal(res.intersection, glm::vec3(0.0f, 0.0f, -0.5f)));

	ray.origin = Maths::up_vec * 5.0f;
	ray.direction = -Maths::up_vec;
	res = ecs.check_any_entity_clicked(ray);
	ASSERT_TRUE(res.bCollided);
	ASSERT_EQ(res.id, object1.get_id());
	ASSERT_TRUE(glm_equal(res.intersection, glm::vec3(0.0f, 0.5f, 0.0f)));
}

TEST_F(ClickableECSFixture, hit_obj2)
{
	Maths::Ray ray{ glm::vec3(1.0f, 1.0f, -5.0f), Maths::forward_vec };
	auto res = ecs.check_any_entity_clicked(ray);
	ASSERT_TRUE(res.bCollided);
	ASSERT_EQ(res.id, object2.get_id());
	ASSERT_TRUE(glm_equal(res.intersection, glm::vec3(1.0f, 1.0f, 0.5f)));

	ray.origin = glm::vec3(1.0f, -1.0f, 1.0f);
	ray.direction = Maths::up_vec;
	res = ecs.check_any_entity_clicked(ray);
	ASSERT_TRUE(res.bCollided);
	ASSERT_EQ(res.id, object2.get_id());
	ASSERT_TRUE(glm_equal(res.intersection, glm::vec3(1.0f, 0.5f, 1.0f)));
}

TEST_F(ClickableECSFixture, hit_none)
{
	Maths::Ray ray{ glm::vec3(-1.0f, 0.0f, -5.0f), Maths::forward_vec };
	auto res = ecs.check_any_entity_clicked(ray);
	ASSERT_FALSE(res.bCollided);
}

TEST(ClickableECS, falls_back_to_an_entity_collider_without_a_rigid_body)
{
	ECS ecs;
	Object object;
	ecs.add_object(object);
	ecs.add_collider(object.get_id(), std::make_unique<SphereCollider>());
	ecs.add_clickable_entity(object.get_id());

	const auto hit = ecs.check_any_entity_clicked(
		Maths::Ray(-Maths::forward_vec, Maths::forward_vec));
	ASSERT_TRUE(hit.bCollided);
	EXPECT_EQ(hit.id, object.get_id());
}

TEST(ClickableECS, rejects_an_entity_that_is_not_registered_with_the_ecs)
{
	ECS ecs;
	EXPECT_THROW(
		ecs.add_clickable_entity(
			EntityID(std::numeric_limits<std::uint64_t>::max())),
		std::invalid_argument);
}

TEST(ClickableECS, rejects_a_deserialized_entity_that_is_not_registered_with_the_ecs)
{
	ECS ecs;
	const auto serialized = Deserializer::parse(
		"clickable_system:\n"
		"  - entity_id: 18446744073709551615\n");
	EXPECT_THROW(
		ecs.ClickableSystem::deserialize(serialized),
		SerializationError);
}

TEST(HoverableECS, falls_back_to_an_entity_collider_without_a_rigid_body)
{
	ECS ecs;
	Object object;
	ecs.add_object(object);
	ecs.add_collider(object.get_id(), std::make_unique<SphereCollider>());
	ecs.add_hoverable_entity(object.get_id());

	const auto hit = ecs.check_any_entity_hovered(
		Maths::Ray(-Maths::forward_vec, Maths::forward_vec));
	ASSERT_TRUE(hit.bCollided);
	EXPECT_EQ(hit.id, object.get_id());
}

TEST(HoverableECS, removing_an_object_removes_its_hoverable_membership)
{
	ECS ecs;
	Object object;
	ecs.add_object(object);
	ecs.add_collider(object.get_id(), std::make_unique<SphereCollider>());
	ecs.add_hoverable_entity(object.get_id());
	ecs.remove_object(object.get_id());

	Serializer serializer;
	ecs.HoverableSystem::serialize(serializer);
	EXPECT_TRUE(Deserializer::parse(serializer.emit())
		.child("hoverable_system").elements().empty());
}

TEST_F(ClickableECSFixture, hit_both)
{
	Maths::Ray ray{ glm::vec3(-1.0f), glm::vec3(1.0f) };
	auto res = ecs.check_any_entity_clicked(ray);
	ASSERT_TRUE(res.bCollided);
	ASSERT_EQ(res.id, object1.get_id());
	ASSERT_TRUE(glm_equal(res.intersection, 0.5f * glm::normalize(glm::vec3(-1.0f))));

	ray.origin = glm::vec3(2.0f);
	ray.direction = glm::vec3(-1.0f);
	res = ecs.check_any_entity_clicked(ray);
	ASSERT_TRUE(res.bCollided);
	ASSERT_EQ(res.id, object2.get_id());
	ASSERT_TRUE(glm_equal(res.intersection, 1.0f + 0.5f * glm::normalize(glm::vec3(1.0f))));
}

TEST_F(ClickableECSFixture, serialization_round_trip_replaces_existing_state)
{
	Serializer serializer;
	ecs.ClickableSystem::serialize(serializer);

	const auto serialized = Deserializer::parse(serializer.emit());
	const auto entries = serialized.child("clickable_system").elements();
	ASSERT_EQ(entries.size(), 2);
	std::unordered_set<std::uint64_t> ids;
	for (const auto& entry : entries)
		ids.insert(entry.read<std::uint64_t>("entity_id"));
	EXPECT_EQ(ids, (std::unordered_set<std::uint64_t>{
		object1.get_id().get_underlying(), object2.get_id().get_underlying() }));

	ECS restored;
	Object replaced_object;
	restored.add_object(object1);
	restored.add_object(object2);
	restored.add_object(replaced_object);
	restored.add_collider(replaced_object.get_id(), std::make_unique<SphereCollider>());
	restored.add_clickable_entity(replaced_object.get_id());
	restored.ClickableSystem::deserialize(serialized);

	Serializer restored_serializer;
	restored.ClickableSystem::serialize(restored_serializer);
	const auto restored_entries = Deserializer::parse(restored_serializer.emit())
		.child("clickable_system").elements();
	ASSERT_EQ(restored_entries.size(), 2);
	for (const auto& entry : restored_entries)
		EXPECT_TRUE(ids.contains(entry.read<std::uint64_t>("entity_id")));
}

TEST_F(ClickableECSFixture, deserialization_reports_field_path_and_preserves_state)
{
	const auto malformed = Deserializer::parse(
		"clickable_system:\n"
		"  - entity_id: invalid\n");
	try {
		ecs.ClickableSystem::deserialize(malformed);
		FAIL() << "Expected invalid entity ID to fail";
	} catch (const SerializationError& error) {
		EXPECT_NE(std::string(error.what()).find("$.clickable_system[0].entity_id"), std::string::npos);
	}

	Serializer serializer;
	ecs.ClickableSystem::serialize(serializer);
	EXPECT_EQ(Deserializer::parse(serializer.emit()).child("clickable_system").elements().size(), 2);
}
