#include <entity_component_system/ecs.hpp>
#include <objects/object.hpp>

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
