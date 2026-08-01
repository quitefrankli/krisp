#include "test_helper.hpp"

#include <collision/collider.hpp>
#include <collision/collision_detector.hpp>
#include <entity_component_system/mesh_system.hpp>
#include <renderable/mesh_factory.hpp>

#include <gtest/gtest.h>

namespace
{
MeshHandle add_test_mesh(
	MeshSystem& meshes, const std::initializer_list<glm::vec3> positions,
	std::vector<uint32_t> indices)
{
    ColorVertices vertices;
    vertices.reserve(positions.size());
    for (const auto& position : positions)
        vertices.push_back({ .pos = position });
    return meshes.add(std::make_unique<ColorMesh>(std::move(vertices), std::move(indices)));
}
}


TEST(collider_tests, quad_collider_hits_inside_unit_square)
{
    const QuadCollider quad(Maths::Plane(glm::vec3(0.0f), Maths::forward_vec), glm::vec2(1.0f));
    const RayCollider ray(Maths::Ray(glm::vec3(0.25f, -0.25f, -1.0f), Maths::forward_vec));

    glm::vec3 intersection;
    ASSERT_TRUE(quad.check_collision(ray, intersection));
    ASSERT_TRUE(glm_equal(intersection, glm::vec3(0.25f, -0.25f, 0.0f)));
}

TEST(collider_tests, quad_collider_rejects_outside_bounds_but_respects_transform_scale)
{
    QuadCollider quad(Maths::Plane(glm::vec3(0.0f), Maths::forward_vec), glm::vec2(1.0f));

    const RayCollider miss_ray(Maths::Ray(glm::vec3(0.6f, 0.0f, -1.0f), Maths::forward_vec));
    glm::vec3 intersection;
    ASSERT_FALSE(quad.check_collision(miss_ray, intersection));

    quad.set_temporary_transform(Maths::Transform(
        glm::vec3(0.0f, 0.0f, 2.0f),
        glm::vec3(4.0f, 4.0f, 1.0f),
        Maths::identity_quat));

    const RayCollider hit_ray(Maths::Ray(glm::vec3(1.5f, 0.0f, 0.0f), Maths::forward_vec));
    ASSERT_TRUE(quad.check_collision(hit_ray, intersection));
    ASSERT_TRUE(glm_equal(intersection, glm::vec3(1.5f, 0.0f, 2.0f)));
}

TEST(collider_tests, collision_detector_dispatches_ray_quad_via_registry)
{
    const RayCollider ray(Maths::Ray(glm::vec3(0.0f, 0.0f, -1.0f), Maths::forward_vec));
    const QuadCollider quad(Maths::Plane(glm::vec3(0.0f), Maths::forward_vec), glm::vec2(1.0f));

    const CollisionResult result = CollisionDetector::check_collision(&ray, &quad);
    ASSERT_TRUE(result.bCollided);
    ASSERT_TRUE(glm_equal(result.intersection, glm::vec3(0.0f, 0.0f, 0.0f)));

    const CollisionResult reverse_result = CollisionDetector::check_collision(&quad, &ray);
    ASSERT_TRUE(reverse_result.bCollided);
    ASSERT_TRUE(glm_equal(reverse_result.intersection, glm::vec3(0.0f, 0.0f, 0.0f)));
}

TEST(collider_tests, box_collider_hits_rotated_non_uniformly_scaled_bounds)
{
    BoxCollider box;
    box.set_temporary_transform(Maths::Transform(
        glm::vec3(0.0f), glm::vec3(2.0f, 1.0f, 1.0f),
        glm::angleAxis(glm::half_pi<float>(), Maths::forward_vec)));

    const RayCollider ray(Maths::Ray(glm::vec3(0.0f, -3.0f, 0.0f), Maths::up_vec));
    const CollisionResult result = CollisionDetector::check_collision(&ray, &box);
    ASSERT_TRUE(result.bCollided);
    ASSERT_TRUE(glm_equal(result.intersection, glm::vec3(0.0f, -1.0f, 0.0f)));
}

TEST(collider_tests, box_collider_handles_parallel_ray_axes_and_inside_origins)
{
    const BoxCollider box;
    RayCollider ray(Maths::Ray(glm::vec3(0.0f, 0.0f, -2.0f), Maths::forward_vec));
    glm::vec3 intersection;
    ASSERT_TRUE(box.check_collision(ray, intersection));
    ASSERT_TRUE(glm_equal(intersection, glm::vec3(0.0f, 0.0f, -0.5f)));

    ray = RayCollider(Maths::Ray(glm::vec3(0.0f), Maths::forward_vec));
    ASSERT_TRUE(box.check_collision(ray, intersection));
    ASSERT_TRUE(glm_equal(intersection, glm::vec3(0.0f)));
}

TEST(collider_tests, capsule_collider_hits_side_and_caps)
{
    const CapsuleCollider capsule(0.5f, 2.0f);

    RayCollider ray(Maths::Ray({ 0.0f, 1.0f, -2.0f }, Maths::forward_vec));
    CollisionResult result = CollisionDetector::check_collision(&ray, &capsule);
    ASSERT_TRUE(result.bCollided);
    EXPECT_TRUE(glm_equal(result.intersection, { 0.0f, 1.0f, -0.5f }));

    ray = RayCollider(Maths::Ray({ 0.0f, 3.0f, 0.0f }, -Maths::up_vec));
    result = CollisionDetector::check_collision(&ray, &capsule);
    ASSERT_TRUE(result.bCollided);
    EXPECT_TRUE(glm_equal(result.intersection, { 0.0f, 2.0f, 0.0f }));

    ray = RayCollider(Maths::Ray({ 0.6f, 1.0f, -2.0f }, Maths::forward_vec));
    result = CollisionDetector::check_collision(&ray, &capsule);
    EXPECT_FALSE(result.bCollided);
}

TEST(collider_tests, capsule_collider_bottom_rests_at_local_y_zero)
{
    const CapsuleCollider capsule(0.5f, 2.0f);
    const RayCollider ray(Maths::Ray({ 0.0f, -1.0f, 0.0f }, Maths::up_vec));

    const CollisionResult result = CollisionDetector::check_collision(&ray, &capsule);

    ASSERT_TRUE(result.bCollided);
    EXPECT_TRUE(glm_equal(result.intersection, Maths::zero_vec));
}

TEST(collider_tests, mesh_collider_rejects_aabb_hits_outside_the_mesh_triangles)
{
	MeshSystem meshes;
    auto mesh_owner = add_test_mesh(meshes, {
        { -1.0f, -1.0f, 0.0f }, { 1.0f, -1.0f, 0.0f }, { -1.0f, 1.0f, 0.0f }
    }, { 0, 1, 2 });
    const MeshCollider mesh({ mesh_owner });
    const RayCollider hit_ray(Maths::Ray(glm::vec3(-0.5f, -0.5f, -1.0f), Maths::forward_vec));
    const RayCollider gap_ray(Maths::Ray(glm::vec3(0.75f, 0.75f, -1.0f), Maths::forward_vec));
    glm::vec3 intersection;

    ASSERT_TRUE(mesh.check_collision(hit_ray, intersection));
    ASSERT_TRUE(glm_equal(intersection, glm::vec3(-0.5f, -0.5f, 0.0f)));
    ASSERT_FALSE(mesh.check_collision(gap_ray, intersection));
}

TEST(collider_tests, mesh_collider_returns_the_closest_triangle_across_meshes_and_transforms)
{
	MeshSystem meshes;
    auto near_owner = add_test_mesh(meshes, {
        { -1.0f, -1.0f, 0.0f }, { 1.0f, -1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }
    }, { 0, 1, 2 });
    auto far_owner = add_test_mesh(meshes, {
        { -1.0f, -1.0f, 1.0f }, { 1.0f, -1.0f, 1.0f }, { 0.0f, 1.0f, 1.0f }
    }, { 0, 1, 2 });
    MeshCollider mesh({ near_owner, far_owner });
    mesh.set_temporary_transform(Maths::Transform(
        glm::vec3(2.0f, 0.0f, 0.0f), glm::vec3(2.0f, 1.0f, 1.0f), Maths::identity_quat));
    const RayCollider ray(Maths::Ray(glm::vec3(2.0f, 0.0f, -1.0f), Maths::forward_vec));
    glm::vec3 intersection;

    ASSERT_TRUE(mesh.check_collision(ray, intersection));
    ASSERT_TRUE(glm_equal(intersection, glm::vec3(2.0f, 0.0f, 0.0f)));
}

TEST(collider_tests, mesh_collider_traverses_the_bvh_for_multi_triangle_meshes)
{
	MeshSystem meshes;
    const auto cube_owner = meshes.add(MeshFactory::cube());
    const auto& pick_data = cube_owner->get().get_pick_data();
    ASSERT_GT(pick_data.get_nodes().size(), 1u);

    const MeshCollider mesh({ cube_owner });
    const RayCollider ray(Maths::Ray(glm::vec3(0.0f, 0.0f, -2.0f), Maths::forward_vec));
    glm::vec3 intersection;
    ASSERT_TRUE(mesh.check_collision(ray, intersection));
    ASSERT_TRUE(glm_equal(intersection, glm::vec3(0.0f, 0.0f, -0.5f)));
}

TEST(collider_tests, mesh_collider_retains_its_mesh_resource)
{
	MeshSystem meshes;
	auto owner = meshes.add(MeshFactory::cube());
	const MeshID id = owner->get_id();
	{
		const MeshCollider collider({ owner });
		owner.reset();
		EXPECT_TRUE(meshes.contains(id));
	}
	EXPECT_FALSE(meshes.contains(id));
}

TEST(collider_tests, mesh_pick_data_ignores_invalid_and_degenerate_triangles)
{
    const std::vector<glm::vec3> positions{
        { 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 2.0f, 0.0f, 0.0f }
    };
    const MeshPickData data(positions, { 0, 1, 2, 0, 1, 99 });
    ASSERT_TRUE(data.has_bounds());
    ASSERT_FALSE(data.has_triangles());
}

TEST(collider_tests, tiled_quad_with_various_rays)
{
    // 1x1 quad centered at origin, facing up
    const QuadCollider quad(Maths::Plane(glm::vec3(0.0f), Maths::up_vec), glm::vec2(1.0f));

    // ray pointing downwards, should hit
    RayCollider ray(Maths::Ray(glm::vec3(0.0f, 1.0f, 0.0f), -Maths::up_vec));
    auto result = CollisionDetector::check_collision(&ray, &quad);
    ASSERT_TRUE(result.bCollided);
    ASSERT_TRUE(glm_equal(result.intersection, glm::vec3(0.0f, 0.0f, 0.0f)));

    // ray pointing upwards, should miss
    ray = RayCollider(Maths::Ray(glm::vec3(0.0f, 1.0f, 0.0f), Maths::up_vec));
    result = CollisionDetector::check_collision(&ray, &quad);
    ASSERT_FALSE(result.bCollided);

    // ray pointing downwards but offset, should miss
    ray = RayCollider(Maths::Ray(glm::vec3(1.0f, 1.0f, 0.0f), -Maths::up_vec));
    result = CollisionDetector::check_collision(&ray, &quad);
    ASSERT_FALSE(result.bCollided);

    // ray pointing downwards but offset at edge, should hit
    ray = RayCollider(Maths::Ray(glm::vec3(0.48f, 1.0f, 0.0f), -Maths::up_vec));
    result = CollisionDetector::check_collision(&ray, &quad);
    ASSERT_TRUE(result.bCollided);
    ASSERT_TRUE(glm_equal(result.intersection, glm::vec3(0.48f, 0.0f, 0.0f)));

    // ray parallel to plane, should miss
    ray = RayCollider(Maths::Ray(glm::vec3(0.0f, 1.0f, 0.0f), Maths::right_vec));
    result = CollisionDetector::check_collision(&ray, &quad);
    ASSERT_FALSE(result.bCollided);

    // ray pointing downwards but starting below plane, should miss
    ray = RayCollider(Maths::Ray(glm::vec3(0.0f, -1.0f, 0.0f), -Maths::up_vec));
    result = CollisionDetector::check_collision(&ray, &quad);
    ASSERT_FALSE(result.bCollided);

    // ray pointing diagonally downwards, should hit
    ray = RayCollider(Maths::Ray(glm::vec3(0.5f, 1.0f, 0.0f), glm::normalize(glm::vec3(-0.5f, -1.0f, 0.0f))));
    result = CollisionDetector::check_collision(&ray, &quad);
    ASSERT_TRUE(result.bCollided);
    ASSERT_TRUE(glm_equal(result.intersection, glm::vec3(0.0f, 0.0f, 0.0f)));

    // ray pointing diagonally downwards but big angle, should miss
    ray = RayCollider(Maths::Ray(glm::vec3(0.5f, 1.0f, 0.0f), glm::normalize(glm::vec3(-0.5f, -0.1f, 0.0f))));
    result = CollisionDetector::check_collision(&ray, &quad);
    ASSERT_FALSE(result.bCollided);
}
