#include <entity_component_system/ecs.hpp>
#include <objects/object.hpp>
#include <serialization/serializer.hpp>
#include <serialization/serialization_helpers.hpp>

#include <algorithm>
#include <limits>
#include <gtest/gtest.h>

namespace {
struct PhysicsECS : ECS
{
	Object object;
	PhysicsECS() { add_object(object); }
};

TriangleMeshPhysicsShape make_floor_mesh()
{
	return {
		.vertices = {{-10.0f, 0.0f, -10.0f}, {-10.0f, 0.0f, 10.0f}, {10.0f, 0.0f, 10.0f}, {10.0f, 0.0f, -10.0f}},
		.indices = {0, 1, 2, 0, 2, 3},
	};
}
}

TEST(JoltPhysics, StaticTriangleMeshSupportsFloorCollision)
{
	PhysicsECS ecs;
	const auto floor = ecs.object.get_id();
	const EntityID ball(7201);
	ecs.add_transformation(ball);
	ecs.set_position(ball, {0.0f, 2.0f, 0.0f});
	ecs.add_rigid_body(floor, RigidBodyDefinition{
		.shape = make_floor_mesh(), .motion = PhysicsMotionType::Static,
	});
	ecs.add_rigid_body(ball, RigidBodyDefinition{
		.shape = SpherePhysicsShape{0.5f}, .motion = PhysicsMotionType::Dynamic,
	});

	for (int frame = 0; frame < 240; ++frame) ecs.process(1.0f / 60.0f);
	EXPECT_NEAR(ecs.get_position(ball).y, 0.5f, 0.03f);
	EXPECT_NEAR(ecs.get_linear_velocity(ball).y, 0.0f, 0.05f);
}

TEST(JoltPhysics, StaticTriangleMeshWallAndRampAreRaycastable)
{
	PhysicsECS ecs;
	const EntityID wall(7202), ramp(7203);
	ecs.add_transformation(wall);
	ecs.add_transformation(ramp);
	ecs.add_rigid_body(wall, RigidBodyDefinition{
		.shape = TriangleMeshPhysicsShape{
			.vertices = {{-1.0f, -1.0f, 0.0f}, {1.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {-1.0f, 1.0f, 0.0f}},
			.indices = {0, 1, 2, 0, 2, 3},
		},
	});
	ecs.set_position(ramp, {4.0f, 0.0f, 0.0f});
	ecs.add_rigid_body(ramp, RigidBodyDefinition{
		.shape = TriangleMeshPhysicsShape{
			.vertices = {{-2.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}, {2.0f, 2.0f, 4.0f}, {-2.0f, 2.0f, 4.0f}},
			.indices = {0, 2, 1, 0, 3, 2},
		},
	});

	const auto wall_hit = ecs.PhysicsSystem::raycast(Maths::Ray({0.0f, 0.0f, 2.0f}, {0.0f, 0.0f, -1.0f}));
	ASSERT_TRUE(wall_hit.bCollided);
	EXPECT_EQ(wall_hit.id, wall);
	const auto ramp_hit = ecs.PhysicsSystem::raycast(Maths::Ray({4.0f, 5.0f, 2.0f}, {0.0f, -1.0f, 0.0f}));
	ASSERT_TRUE(ramp_hit.bCollided);
	EXPECT_EQ(ramp_hit.id, ramp);
	EXPECT_NEAR(ramp_hit.intersection.y, 1.0f, 0.01f);
}

TEST(JoltPhysics, TriangleMeshHonoursEntityTransformAndShapeOffset)
{
	PhysicsECS ecs;
	const auto id = ecs.object.get_id();
	const glm::vec3 origin(4.0f, 2.0f, 0.0f);
	ecs.set_position(id, origin);
	ecs.set_rotation(id, glm::angleAxis(Maths::PI * 0.5f, Maths::up_vec));
	ecs.add_rigid_body(id, RigidBodyDefinition{
		.shape = TriangleMeshPhysicsShape{
			.vertices = {{-1.0f, -1.0f, 0.0f}, {1.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {-1.0f, 1.0f, 0.0f}},
			.indices = {0, 1, 2, 0, 2, 3},
		},
		.shape_offset = {0.0f, 0.0f, 1.0f},
	});

	const auto hit = ecs.PhysicsSystem::raycast(Maths::Ray({8.0f, 2.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}));
	ASSERT_TRUE(hit.bCollided);
	EXPECT_EQ(hit.id, id);
	EXPECT_NEAR(hit.intersection.x, 5.0f, 0.01f);
	EXPECT_EQ(ecs.get_position(id), origin);
	const auto bodies = ecs.get_debug_bodies();
	ASSERT_EQ(bodies.size(), 1);
	EXPECT_NEAR(bodies.front().position.x, 5.0f, 0.001f);
}

TEST(JoltPhysics, TriangleMeshCanBeDisabledRemovedAndRecreated)
{
	PhysicsECS ecs;
	const auto id = ecs.object.get_id();
	const Maths::Ray ray({0.0f, 2.0f, 0.0f}, {0.0f, -1.0f, 0.0f});
	auto add_mesh = [&] {
		ecs.add_rigid_body(id, RigidBodyDefinition{.shape = make_floor_mesh()});
	};
	add_mesh();
	EXPECT_TRUE(ecs.PhysicsSystem::raycast(ray).bCollided);
	EXPECT_FALSE(ecs.get_debug_shape_triangles(id).empty());
	const auto first_body = ecs.get_debug_bodies().front().body_id;
	ecs.set_body_enabled(id, false);
	EXPECT_FALSE(ecs.PhysicsSystem::raycast(ray).bCollided);
	ecs.set_body_enabled(id, true);
	EXPECT_TRUE(ecs.PhysicsSystem::raycast(ray).bCollided);
	ecs.remove_rigid_body(id);
	EXPECT_FALSE(ecs.has_rigid_body(id));
	EXPECT_FALSE(ecs.PhysicsSystem::raycast(ray).bCollided);
	add_mesh();
	EXPECT_TRUE(ecs.PhysicsSystem::raycast(ray).bCollided);
	ASSERT_EQ(ecs.get_debug_bodies().size(), 1);
	EXPECT_NE(ecs.get_debug_bodies().front().body_id, first_body);
}

TEST(JoltPhysics, InvalidTriangleMeshesAreRejectedWithoutReplacingExistingBody)
{
	PhysicsECS ecs;
	const auto id = ecs.object.get_id();
	ecs.add_rigid_body(id, RigidBodyDefinition{.shape = SpherePhysicsShape{0.5f}});
	const Maths::Ray ray({0.0f, 0.0f, -2.0f}, {0.0f, 0.0f, 1.0f});
	const float nan = std::numeric_limits<float>::quiet_NaN();
	const std::vector<TriangleMeshPhysicsShape> invalid_shapes{
		{},
		{.vertices = {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}, .indices = {0, 1, 3}},
		{.vertices = {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}, .indices = {0, 1}},
		{.vertices = {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}}, .indices = {0, 1, 2}},
		{.vertices = {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, nan, 0.0f}}, .indices = {0, 1, 2}},
	};
	for (const auto& mesh : invalid_shapes) {
		EXPECT_THROW(ecs.add_rigid_body(id, RigidBodyDefinition{.shape = mesh}), std::invalid_argument);
		EXPECT_TRUE(ecs.has_rigid_body(id));
		const auto existing_hit = ecs.PhysicsSystem::raycast(ray);
		ASSERT_TRUE(existing_hit.bCollided);
		EXPECT_EQ(existing_hit.id, id);
	}
	for (const auto motion : {PhysicsMotionType::Kinematic, PhysicsMotionType::Dynamic}) {
		EXPECT_THROW(ecs.add_rigid_body(id, RigidBodyDefinition{
			.shape = make_floor_mesh(), .motion = motion,
		}), std::invalid_argument);
	}
}

TEST(JoltPhysicsSerialization, TriangleMeshRoundTripsAndMalformedMeshPreservesCurrentState)
{
	PhysicsECS source;
	const auto id = source.object.get_id();
	source.add_rigid_body(id, RigidBodyDefinition{.shape = make_floor_mesh()});
	Serializer serialized;
	source.TransformationSystem::serialize(serialized);
	source.PhysicsSystem::serialize(serialized);
	const auto saved = Deserializer::parse(serialized.emit());
	const auto body = saved.child("physics_system").child("bodies").elements().front();
	EXPECT_EQ(body.read<int>("shape"), 3);
	EXPECT_EQ(body.child("vertices").elements().size(), 4);
	EXPECT_EQ(body.child("indices").elements().size(), 6);

	PhysicsECS restored;
	restored.TransformationSystem::deserialize(saved);
	restored.PhysicsSystem::deserialize(saved);
	const Maths::Ray floor_ray({0.0f, 2.0f, 0.0f}, {0.0f, -1.0f, 0.0f});
	const auto restored_hit = restored.PhysicsSystem::raycast(floor_ray);
	ASSERT_TRUE(restored_hit.bCollided);
	EXPECT_EQ(restored_hit.id, id);
	EXPECT_FALSE(restored.get_debug_shape_triangles(id).empty());

	PhysicsECS existing;
	const auto existing_id = existing.object.get_id();
	existing.add_rigid_body(existing_id, RigidBodyDefinition{.shape = SpherePhysicsShape{0.5f}});
	Serializer malformed;
	auto system = malformed.map("physics_system");
	Serialization::write_vec3(system, "gravity", {0.0f, -9.8f, 0.0f});
	auto entry = system.sequence("bodies").append_map();
	entry.write("entity_id", existing_id.get_underlying());
	entry.write("shape", 3);
	auto vertices = entry.sequence("vertices");
	for (const glm::vec3 vertex : {glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)}) {
		auto serialized_vertex = vertices.append_map();
		serialized_vertex.write("x", vertex.x);
		serialized_vertex.write("y", vertex.y);
		serialized_vertex.write("z", vertex.z);
	}
	auto indices = entry.sequence("indices");
	indices.append(0); indices.append(1); indices.append(3);
	Serialization::write_vec3(entry, "shape_offset", {0.0f, 0.0f, 0.0f});
	entry.write("motion", static_cast<int>(PhysicsMotionType::Static));
	entry.write("quality", static_cast<int>(PhysicsMotionQuality::Discrete));
	entry.write("participation", static_cast<int>(PhysicsParticipation::Solid));
	entry.write("persistence", static_cast<int>(PhysicsPersistence::Persistent));
	entry.write("mass", 1.0f); entry.write("friction", 0.5f); entry.write("restitution", 0.0f);
	entry.write("linear_damping", 0.05f); entry.write("angular_damping", 0.05f); entry.write("gravity_factor", 1.0f);
	entry.write("enabled", true); entry.write("active", false);
	Serialization::write_vec3(entry, "linear_velocity", {0.0f, 0.0f, 0.0f});
	Serialization::write_vec3(entry, "angular_velocity", {0.0f, 0.0f, 0.0f});
	system.sequence("contact_restitution_overrides");
	EXPECT_THROW(existing.PhysicsSystem::deserialize(Deserializer::parse(malformed.emit())), SerializationError);
	EXPECT_TRUE(existing.has_rigid_body(existing_id));
	const auto existing_hit = existing.PhysicsSystem::raycast(Maths::Ray({0.0f, 0.0f, -2.0f}, {0.0f, 0.0f, 1.0f}));
	ASSERT_TRUE(existing_hit.bCollided);
	EXPECT_EQ(existing_hit.id, existing_id);
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

TEST(JoltPhysics, ShapeOffsetRotatesWithEntityAndPreservesEntityOrigin)
{
	PhysicsECS ecs;
	const auto id = ecs.object.get_id();
	const glm::vec3 origin(4.0f, 2.0f, 0.0f);
	ecs.set_position(id, origin);
	ecs.set_rotation(id, glm::angleAxis(Maths::PI * 0.5f, Maths::forward_vec));
	ecs.add_rigid_body(id, RigidBodyDefinition{
		.shape = SpherePhysicsShape{0.25f}, .motion = PhysicsMotionType::Kinematic,
		.shape_offset = {0.0f, 1.0f, 0.0f},
	});
	ecs.process(1.0f / 60.0f);
	EXPECT_EQ(ecs.get_position(id), origin);
	EXPECT_FALSE(ecs.get_debug_shape_triangles(id).empty());
	const auto centre = ecs.get_debug_bodies().front().position;
	EXPECT_NEAR(centre.x, 3.0f, 0.001f);
	EXPECT_NEAR(centre.y, 2.0f, 0.001f);
	const Maths::Ray ray({3.0f, 2.0f, -3.0f}, Maths::forward_vec);
	EXPECT_TRUE(ecs.PhysicsSystem::raycast(ray).bCollided);
	const std::vector<EntityID> candidates{id};
	EXPECT_TRUE(ecs.PhysicsSystem::raycast(ray, candidates).bCollided);
	EXPECT_FALSE(ecs.PhysicsSystem::raycast(Maths::Ray({4.0f, 2.0f, -3.0f}, Maths::forward_vec), candidates).bCollided);

	Serializer saved;
	ecs.TransformationSystem::serialize(saved);
	ecs.PhysicsSystem::serialize(saved);
	PhysicsECS restored;
	const auto document = Deserializer::parse(saved.emit());
	restored.TransformationSystem::deserialize(document);
	restored.PhysicsSystem::deserialize(document);
	EXPECT_EQ(restored.get_position(id), origin);
	EXPECT_TRUE(restored.PhysicsSystem::raycast(ray, candidates).bCollided);
	EXPECT_NEAR(restored.get_debug_bodies().front().position.x, 3.0f, 0.001f);
}

namespace {
CharacterControllerDefinition test_character_controller()
{
	return CharacterControllerDefinition{
		.radius = 0.35f,
		.height = 1.9f,
		.max_slope_angle = glm::radians(45.0f),
		.step_height = 0.3f,
		.ground_snap_distance = 0.1f,
	};
}

TriangleMeshPhysicsShape make_controller_floor(float half_extent = 10.0f)
{
	return {
		.vertices = {{-half_extent, 0.0f, -half_extent}, {-half_extent, 0.0f, half_extent}, {half_extent, 0.0f, half_extent}, {half_extent, 0.0f, -half_extent}},
		.indices = {0, 1, 2, 0, 2, 3},
	};
}
}

TEST(JoltCharacterController, FallsUnderGravityAndLandsWithFeetAboveStaticMesh)
{
	PhysicsECS ecs;
	const auto floor = ecs.object.get_id();
	const EntityID character(7301);
	ecs.add_transformation(character);
	ecs.set_position(character, {0.0f, 1.52f, 0.0f});
	ecs.add_rigid_body(floor, RigidBodyDefinition{.shape = make_controller_floor()});
	ecs.add_character_controller(character, test_character_controller());

	ecs.process(1.0f / 60.0f);
	EXPECT_NEAR(ecs.get_position(character).y, 1.52f, 0.03f);
	EXPECT_LT(ecs.get_character_velocity(character).y, 0.0f);
	EXPECT_FALSE(ecs.is_character_grounded(character));

	for (int frame = 0; frame < 180; ++frame) ecs.process(1.0f / 60.0f);
	EXPECT_TRUE(ecs.is_character_grounded(character));
	EXPECT_NEAR(ecs.get_position(character).y, 0.0f, 0.05f);
	EXPECT_NEAR(ecs.get_character_velocity(character).y, 0.0f, 0.05f);
	EXPECT_FALSE(ecs.has_rigid_body(character));

	const auto feet_hit = ecs.PhysicsSystem::raycast(Maths::Ray({-0.5f, 0.1f, 0.0f}, {1.0f, 0.0f, 0.0f}));
	ASSERT_TRUE(feet_hit.bCollided);
	EXPECT_EQ(feet_hit.id, character);
	EXPECT_NEAR(feet_hit.intersection.y, 0.1f, 0.01f);
}

TEST(JoltCharacterController, DesiredPlanarVelocityPreservesGravityAndZeroStepDoesNotMove)
{
	PhysicsECS ecs;
	const EntityID character(7302);
	ecs.add_transformation(character);
	ecs.set_position(character, {0.0f, 5.0f, 0.0f});
	ecs.add_character_controller(character, test_character_controller());
	ecs.set_character_desired_velocity(character, {2.0f, 0.0f, 0.0f});
	ecs.process(1.0f / 60.0f);
	const auto position_after_step = ecs.get_position(character);
	const auto velocity_after_step = ecs.get_character_velocity(character);
	EXPECT_GT(velocity_after_step.x, 0.0f);
	EXPECT_LT(velocity_after_step.y, 0.0f);

	ecs.process(0.0f);
	EXPECT_EQ(ecs.get_position(character), position_after_step);
	EXPECT_EQ(ecs.get_character_velocity(character), velocity_after_step);
}

TEST(JoltCharacterController, LaunchIsGroundedOnlyAndAllowsRelaunchAfterLanding)
{
	PhysicsECS ecs;
	const auto floor = ecs.object.get_id();
	const EntityID character(7303);
	ecs.add_transformation(character);
	ecs.add_rigid_body(floor, RigidBodyDefinition{.shape = make_controller_floor()});
	ecs.add_character_controller(character, test_character_controller());
	for (int frame = 0; frame < 4; ++frame) ecs.process(1.0f / 60.0f);
	ASSERT_TRUE(ecs.is_character_grounded(character));

	EXPECT_THROW(ecs.launch_character(character, 0.0f), std::invalid_argument);
	ASSERT_TRUE(ecs.launch_character(character, 5.0f));
	const auto launched_y = ecs.get_position(character).y;
	EXPECT_FALSE(ecs.launch_character(character, 5.0f));
	for (int frame = 0; frame < 20; ++frame) ecs.process(1.0f / 60.0f);
	EXPECT_GT(ecs.get_position(character).y, launched_y);
	EXPECT_FALSE(ecs.is_character_grounded(character));

	for (int frame = 0; frame < 180 && !ecs.is_character_grounded(character); ++frame)
		ecs.process(1.0f / 60.0f);
	ASSERT_TRUE(ecs.is_character_grounded(character));
	EXPECT_TRUE(ecs.launch_character(character, 5.0f));
	EXPECT_FALSE(ecs.is_character_grounded(character));
}

TEST(JoltCharacterController, WalkingOffPlatformFallsWithoutLargeGroundSnap)
{
	PhysicsECS ecs;
	const auto platform = ecs.object.get_id();
	const EntityID character(7304);
	ecs.add_transformation(character);
	ecs.set_position(character, {0.0f, 0.0f, 0.0f});
	ecs.add_rigid_body(platform, RigidBodyDefinition{
		.shape = make_controller_floor(1.0f),
		.shape_offset = {0.0f, -0.1f, 0.0f},
	});
	ecs.add_character_controller(character, test_character_controller());
	for (int frame = 0; frame < 30; ++frame) ecs.process(1.0f / 60.0f);
	ASSERT_TRUE(ecs.is_character_grounded(character));

	ecs.set_character_desired_velocity(character, {3.0f, 0.0f, 0.0f});
	for (int frame = 0; frame < 50; ++frame) ecs.process(1.0f / 60.0f);
	EXPECT_GT(ecs.get_position(character).x, 1.0f);
	EXPECT_LT(ecs.get_position(character).y, -0.25f);
	EXPECT_FALSE(ecs.is_character_grounded(character));
}

TEST(JoltCharacterController, SupportsWalkableSlopeWallAndLowStep)
{
	PhysicsECS ecs;
	const EntityID floor(7310), slope(7311), wall(7312), step(7313), character(7314);
	for (const auto id : {floor, slope, wall, step, character}) ecs.add_transformation(id);
	ecs.add_rigid_body(floor, RigidBodyDefinition{.shape = make_controller_floor(20.0f)});
	ecs.set_position(slope, {-6.0f, 0.0f, 0.0f});
	ecs.add_rigid_body(slope, RigidBodyDefinition{
		.shape = TriangleMeshPhysicsShape{
			.vertices = {{-2.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}, {2.0f, 1.0f, 4.0f}, {-2.0f, 1.0f, 4.0f}},
			.indices = {0, 2, 1, 0, 3, 2},
		},
	});
	ecs.set_position(step, {2.0f, 0.0f, 0.0f});
	ecs.add_rigid_body(step, RigidBodyDefinition{.shape = BoxPhysicsShape{{0.5f, 0.15f, 1.0f}}});
	ecs.set_position(wall, {5.0f, 1.0f, 0.0f});
	ecs.add_rigid_body(wall, RigidBodyDefinition{.shape = BoxPhysicsShape{{0.1f, 1.0f, 1.0f}}});
	ecs.set_position(character, {-6.0f, 0.0f, -2.0f});
	ecs.add_character_controller(character, test_character_controller());
	for (int frame = 0; frame < 4; ++frame) ecs.process(1.0f / 60.0f);

	ecs.set_character_desired_velocity(character, {0.0f, 0.0f, 2.0f});
	for (int frame = 0; frame < 120; ++frame) ecs.process(1.0f / 60.0f);
	EXPECT_GT(ecs.get_position(character).y, 0.4f);
	EXPECT_GT(ecs.get_position(character).z, 1.5f);
	EXPECT_TRUE(ecs.is_character_grounded(character));

	ecs.teleport_character_controller(character, {0.0f, 0.0f, 0.0f});
	ecs.set_character_desired_velocity(character, {2.0f, 0.0f, 0.0f});
	for (int frame = 0; frame < 75; ++frame) ecs.process(1.0f / 60.0f);
	EXPECT_GT(ecs.get_position(character).x, 2.0f);
	EXPECT_GE(ecs.get_position(character).y, 0.14f);

	ecs.teleport_character_controller(character, {4.0f, 0.0f, 0.0f});
	ecs.set_character_desired_velocity(character, {3.0f, 0.0f, 0.0f});
	for (int frame = 0; frame < 60; ++frame) ecs.process(1.0f / 60.0f);
	EXPECT_LT(ecs.get_position(character).x, 4.7f);
}

TEST(JoltCharacterController, ValidatesDefinitionsAndEnforcesExclusiveBodyOwnership)
{
	PhysicsECS ecs;
	const EntityID character(7320), body(7321);
	ecs.add_transformation(character);
	ecs.add_transformation(body);
	const float nan = std::numeric_limits<float>::quiet_NaN();
	const float infinity = std::numeric_limits<float>::infinity();
	const std::vector<CharacterControllerDefinition> invalid_definitions{
		{.radius = 0.0f},
		{.radius = -0.1f},
		{.radius = infinity},
		{.height = 0.0f},
		{.height = 0.5f},
		{.height = nan},
		{.max_slope_angle = -0.1f},
		{.max_slope_angle = glm::radians(91.0f)},
		{.step_height = -0.1f},
		{.step_height = nan},
		{.ground_snap_distance = -0.1f},
		{.ground_snap_distance = infinity},
	};
	for (const auto& definition : invalid_definitions)
		EXPECT_THROW(ecs.add_character_controller(character, definition), std::invalid_argument);
	EXPECT_FALSE(ecs.has_character_controller(character));

	ecs.add_rigid_body(body, RigidBodyDefinition{.shape = SpherePhysicsShape{0.5f}});
	EXPECT_THROW(ecs.add_character_controller(body, test_character_controller()), std::invalid_argument);
	ecs.add_character_controller(character, test_character_controller());
	EXPECT_THROW(ecs.add_rigid_body(character, RigidBodyDefinition{}), std::invalid_argument);
	EXPECT_TRUE(ecs.has_character_controller(character));
	EXPECT_FALSE(ecs.has_rigid_body(character));

	ecs.remove_character_controller(character);
	EXPECT_FALSE(ecs.has_character_controller(character));
	EXPECT_NO_THROW(ecs.add_rigid_body(character, RigidBodyDefinition{}));
}

TEST(JoltCharacterController, EntityRemovalCleansUpControllerAndQueryProxy)
{
	PhysicsECS ecs;
	const auto id = ecs.object.get_id();
	ecs.set_position(id, {0.0f, 2.0f, 0.0f});
	ecs.add_character_controller(id, test_character_controller());
	EXPECT_TRUE(ecs.has_character_controller(id));
	EXPECT_FALSE(ecs.has_rigid_body(id));
	EXPECT_TRUE(ecs.PhysicsSystem::raycast(Maths::Ray({0.0f, 4.0f, 0.0f}, {0.0f, -1.0f, 0.0f})).bCollided);
	ecs.remove_object(id);
	EXPECT_FALSE(ecs.has_character_controller(id));
	EXPECT_FALSE(ecs.PhysicsSystem::raycast(Maths::Ray({0.0f, 4.0f, 0.0f}, {0.0f, -1.0f, 0.0f})).bCollided);
}

TEST(JoltCharacterControllerSerialization, RestoresGroundedAndMidairControllerState)
{
	PhysicsECS source;
	const auto floor = source.object.get_id();
	const EntityID grounded(7330), airborne(7331);
	source.add_transformation(grounded);
	source.add_transformation(airborne);
	source.add_rigid_body(floor, RigidBodyDefinition{.shape = make_controller_floor()});
	source.add_character_controller(grounded, test_character_controller());
	source.add_character_controller(airborne, test_character_controller());
	source.set_position(airborne, {2.0f, 5.0f, 0.0f});
	for (int frame = 0; frame < 4; ++frame) source.process(1.0f / 60.0f);
	ASSERT_TRUE(source.is_character_grounded(grounded));
	ASSERT_FALSE(source.is_character_grounded(airborne));
	const auto airborne_velocity = source.get_character_velocity(airborne);

	Serializer serialized;
	source.TransformationSystem::serialize(serialized);
	source.PhysicsSystem::serialize(serialized);
	const auto saved = Deserializer::parse(serialized.emit());
	PhysicsECS restored;
	restored.TransformationSystem::deserialize(saved);
	restored.PhysicsSystem::deserialize(saved);
	EXPECT_TRUE(restored.has_character_controller(grounded));
	EXPECT_TRUE(restored.has_character_controller(airborne));
	EXPECT_TRUE(restored.is_character_grounded(grounded));
	EXPECT_FALSE(restored.is_character_grounded(airborne));
	EXPECT_EQ(restored.get_character_velocity(airborne), airborne_velocity);
	EXPECT_NEAR(restored.get_position(grounded).y, 0.0f, 0.05f);
}

TEST(JoltCharacterControllerSerialization, MalformedControllerRestorePreservesExistingState)
{
	PhysicsECS ecs;
	const auto id = ecs.object.get_id();
	ecs.set_position(id, {9.0f, 3.0f, -2.0f});
	ecs.add_character_controller(id, test_character_controller());
	ecs.set_character_desired_velocity(id, {1.0f, 0.0f, 0.0f});
	ecs.process(1.0f / 60.0f);
	const auto old_position = ecs.get_position(id);
	const auto old_velocity = ecs.get_character_velocity(id);

	Serializer malformed;
	auto system = malformed.map("physics_system");
	Serialization::write_vec3(system, "gravity", {0.0f, -9.8f, 0.0f});
	system.sequence("bodies");
	system.sequence("contact_restitution_overrides");
	auto entry = system.sequence("character_controllers").append_map();
	entry.write("entity_id", id.get_underlying());
	entry.write("radius", -1.0f);
	entry.write("height", 1.9f);
	entry.write("max_slope_angle", glm::radians(45.0f));
	entry.write("step_height", 0.3f);
	entry.write("ground_snap_distance", 0.1f);
	Serialization::write_vec3(entry, "desired_velocity", {0.0f, 0.0f, 0.0f});
	Serialization::write_vec3(entry, "linear_velocity", {0.0f, 0.0f, 0.0f});

	EXPECT_THROW(ecs.PhysicsSystem::deserialize(Deserializer::parse(malformed.emit())), SerializationError);
	EXPECT_TRUE(ecs.has_character_controller(id));
	EXPECT_EQ(ecs.get_position(id), old_position);
	EXPECT_EQ(ecs.get_character_velocity(id), old_velocity);
}
