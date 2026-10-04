#include <entity_component_system/ecs.hpp>
#include <objects/object.hpp>
#include <serialization/serializer.hpp>
#include <serialization/serialization_helpers.hpp>

#include <algorithm>
#include <gtest/gtest.h>

namespace {
struct PhysicsECS : ECS
{
	Object object;
	PhysicsECS() { add_object(object); }
};
}

TEST(JoltPhysics, DynamicBodyFallsAndCanBeReset)
{
	PhysicsECS ecs;
	ecs.set_position(ecs.object.get_id(), {0.0f, 2.0f, 0.0f});
	ecs.add_rigid_body(ecs.object.get_id(), RigidBodyDefinition{
		.shape = SpherePhysicsShape{0.5f}, .motion = PhysicsMotionType::Dynamic,
	});
	ecs.process(1.0f / 60.0f);
	ecs.process(1.0f / 60.0f);
	EXPECT_LT(ecs.get_position(ecs.object.get_id()).y, 2.0f);
	ecs.teleport_body(ecs.object.get_id(), {0.0f, 3.0f, 0.0f});
	EXPECT_EQ(ecs.get_linear_velocity(ecs.object.get_id()), glm::vec3(0.0f));
}

TEST(JoltPhysics, ImpulseMovesBodyAndRaycastFindsIt)
{
	PhysicsECS ecs;
	ecs.set_gravity({0.0f, 0.0f, 0.0f});
	ecs.add_rigid_body(ecs.object.get_id(), RigidBodyDefinition{
		.shape = SpherePhysicsShape{0.5f}, .motion = PhysicsMotionType::Dynamic,
	});
	ecs.add_impulse(ecs.object.get_id(), {1.0f, 0.0f, 0.0f});
	ecs.process(1.0f / 60.0f);
	EXPECT_GT(ecs.get_position(ecs.object.get_id()).x, 0.0f);
	const auto hit = ecs.PhysicsSystem::raycast(Maths::Ray({-2.0f, 0.0f, 0.0f}, Maths::right_vec));
	EXPECT_TRUE(hit.bCollided);
	EXPECT_EQ(hit.id, ecs.object.get_id());
}

TEST(JoltPhysics, DebugGeometryComesFromEnabledJoltShapes)
{
	PhysicsECS ecs;
	ecs.set_position(ecs.object.get_id(), {2.0f, 0.0f, 0.0f});
	ecs.add_rigid_body(ecs.object.get_id(), RigidBodyDefinition{
		.shape = BoxPhysicsShape{{0.5f, 1.0f, 1.5f}},
	});
	const auto bodies = ecs.get_debug_bodies();
	ASSERT_EQ(bodies.size(), 1);
	EXPECT_EQ(bodies.front().entity, ecs.object.get_id());
	EXPECT_EQ(bodies.front().position, glm::vec3(2.0f, 0.0f, 0.0f));
	const auto triangles = ecs.get_debug_shape_triangles(ecs.object.get_id());
	ASSERT_FALSE(triangles.empty());
	for (const auto& triangle : triangles)
		for (const auto& vertex : triangle.vertices)
			EXPECT_GE(vertex.x, -0.5f);

	ecs.set_body_enabled(ecs.object.get_id(), false);
	EXPECT_TRUE(ecs.get_debug_bodies().empty());
	ecs.add_rigid_body(ecs.object.get_id(), RigidBodyDefinition{
		.shape = SpherePhysicsShape{0.25f},
	});
	const auto replacement = ecs.get_debug_bodies();
	ASSERT_EQ(replacement.size(), 1);
	EXPECT_NE(replacement.front().body_id, bodies.front().body_id);
}

TEST(JoltPhysicsSerialization, RestoresBodyDefinitionsAndRuntimeState)
{
	PhysicsECS source;
	const EntityID moving(7001), disabled(7002), transient_body(7003), transient_owner(7004), sleeping(7005);
	for (const auto id : {moving, disabled, sleeping}) source.add_transformation(id);
	source.add_transformation(transient_body);
	source.add_transformation(transient_owner, TransformationPersistence::Transient);
	source.set_position(moving, {3.0f, 4.0f, 5.0f});
	source.add_rigid_body(moving, RigidBodyDefinition{
		.shape = CapsulePhysicsShape{0.3f, 1.2f}, .motion = PhysicsMotionType::Dynamic,
		.quality = PhysicsMotionQuality::Continuous, .participation = PhysicsParticipation::Solid,
		.mass = 2.5f, .friction = 0.7f, .restitution = 0.2f,
		.linear_damping = 0.1f, .angular_damping = 0.2f, .gravity_factor = 0.4f,
	});
	source.set_linear_velocity(moving, {1.0f, 2.0f, 3.0f});
	source.set_angular_velocity(moving, {0.5f, 0.25f, 0.0f});
	source.add_rigid_body(sleeping, RigidBodyDefinition{.shape = SpherePhysicsShape{0.4f}, .motion = PhysicsMotionType::Dynamic});
	source.set_body_active(sleeping, false);
	source.add_rigid_body(disabled, RigidBodyDefinition{
		.shape = BoxPhysicsShape{{0.25f, 0.5f, 0.75f}}, .motion = PhysicsMotionType::Static,
		.enabled = true,
	});
	source.set_body_enabled(disabled, false);
	source.add_rigid_body(transient_body, RigidBodyDefinition{
		.shape = SpherePhysicsShape{0.2f}, .motion = PhysicsMotionType::Dynamic,
		.persistence = PhysicsPersistence::Transient,
	});
	source.add_rigid_body(transient_owner, RigidBodyDefinition{});
	source.set_contact_restitution(moving, disabled, 0.05f);

	Serializer serializer;
	source.TransformationSystem::serialize(serializer);
	source.PhysicsSystem::serialize(serializer);
	const auto saved = Deserializer::parse(serializer.emit());
	const auto saved_bodies = saved.child("physics_system").child("bodies").elements();
	ASSERT_EQ(saved_bodies.size(), 3);
	const auto moving_saved = std::ranges::find_if(saved_bodies, [moving](const Deserializer& entry) {
		return entry.read<std::uint64_t>("entity_id") == moving.get_underlying();
	});
	ASSERT_NE(moving_saved, saved_bodies.end());
	EXPECT_EQ(moving_saved->read<int>("shape"), 2);
	EXPECT_EQ(moving_saved->read<int>("motion"), static_cast<int>(PhysicsMotionType::Dynamic));
	EXPECT_EQ(moving_saved->read<int>("quality"), static_cast<int>(PhysicsMotionQuality::Continuous));
	EXPECT_EQ(moving_saved->read<float>("mass"), 2.5f);
	EXPECT_EQ(moving_saved->read<float>("friction"), 0.7f);
	EXPECT_EQ(moving_saved->read<float>("restitution"), 0.2f);
	EXPECT_EQ(moving_saved->read<float>("gravity_factor"), 0.4f);

	PhysicsECS restored;
	restored.TransformationSystem::deserialize(saved);
	restored.add_transformation(transient_body);
	restored.add_transformation(transient_owner, TransformationPersistence::Transient);
	restored.add_rigid_body(transient_body, RigidBodyDefinition{
		.shape = SpherePhysicsShape{0.2f}, .motion = PhysicsMotionType::Dynamic,
		.persistence = PhysicsPersistence::Transient,
	});
	restored.add_rigid_body(transient_owner, RigidBodyDefinition{});
	restored.PhysicsSystem::deserialize(saved);

	EXPECT_TRUE(restored.has_rigid_body(moving));
	EXPECT_TRUE(restored.has_rigid_body(disabled));
	EXPECT_TRUE(restored.has_rigid_body(sleeping));
	EXPECT_TRUE(restored.has_rigid_body(transient_body));
	EXPECT_TRUE(restored.has_rigid_body(transient_owner));
	EXPECT_FALSE(restored.is_body_enabled(disabled));
	EXPECT_TRUE(restored.is_body_active(moving));
	EXPECT_FALSE(restored.is_body_active(sleeping));
	EXPECT_EQ(restored.get_linear_velocity(moving), glm::vec3(1.0f, 2.0f, 3.0f));
	EXPECT_EQ(restored.get_angular_velocity(moving), glm::vec3(0.5f, 0.25f, 0.0f));
	EXPECT_EQ(restored.get_position(moving), glm::vec3(3.0f, 4.0f, 5.0f));
	EXPECT_FALSE(restored.get_debug_shape_triangles(disabled).empty());

	Serializer roundtrip;
	restored.TransformationSystem::serialize(roundtrip);
	restored.PhysicsSystem::serialize(roundtrip);
	const auto restored_saved = Deserializer::parse(roundtrip.emit());
	EXPECT_FLOAT_EQ(restored_saved.child("physics_system").child("contact_restitution_overrides")
		.elements().front().read<float>("restitution"), 0.05f);
}

TEST(JoltPhysicsSerialization, RejectsMissingBodySequenceAndDuplicateEntities)
{
	PhysicsECS ecs;
	const EntityID body(7101);
	ecs.add_transformation(body);
	ecs.add_rigid_body(body, RigidBodyDefinition{});
	Serializer missing;
	auto missing_system = missing.map("physics_system");
	Serialization::write_vec3(missing_system, "gravity", {0.0f, -9.8f, 0.0f});
	EXPECT_THROW(ecs.PhysicsSystem::deserialize(Deserializer::parse(missing.emit())), SerializationError);

	Serializer duplicate;
	auto system = duplicate.map("physics_system");
	Serialization::write_vec3(system, "gravity", {0.0f, -9.8f, 0.0f});
	auto bodies = system.sequence("bodies");
	auto add_valid_body = [](Serializer& entry, EntityID id, int motion) {
		entry.write("entity_id", id.get_underlying());
		entry.write("shape", 1); entry.write("radius", 0.5f);
		entry.write("motion", motion);
		entry.write("quality", static_cast<int>(PhysicsMotionQuality::Discrete));
		entry.write("participation", static_cast<int>(PhysicsParticipation::Solid));
		entry.write("persistence", static_cast<int>(PhysicsPersistence::Persistent));
		entry.write("mass", 1.0f); entry.write("friction", 0.5f); entry.write("restitution", 0.0f);
		entry.write("linear_damping", 0.05f); entry.write("angular_damping", 0.05f);
		entry.write("gravity_factor", 1.0f); entry.write("enabled", true); entry.write("active", false);
		Serialization::write_vec3(entry, "linear_velocity", {0.0f, 0.0f, 0.0f});
		Serialization::write_vec3(entry, "angular_velocity", {0.0f, 0.0f, 0.0f});
	};
	for (int i = 0; i < 2; ++i) {
		auto entry = bodies.append_map();
		add_valid_body(entry, body, static_cast<int>(PhysicsMotionType::Static));
	}
	EXPECT_THROW(ecs.PhysicsSystem::deserialize(Deserializer::parse(duplicate.emit())), SerializationError);

	Serializer invalid_enum;
	auto invalid_system = invalid_enum.map("physics_system");
	Serialization::write_vec3(invalid_system, "gravity", {0.0f, -9.8f, 0.0f});
	auto invalid_entry = invalid_system.sequence("bodies").append_map();
	add_valid_body(invalid_entry, body, 99);
	EXPECT_THROW(ecs.PhysicsSystem::deserialize(Deserializer::parse(invalid_enum.emit())), SerializationError);
}
