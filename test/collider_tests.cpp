#include "test_helper.hpp"

#include <collision/collider.hpp>
#include <collision/collision_detector.hpp>
#include <entity_component_system/mesh_system.hpp>
#include <renderable/mesh_factory.hpp>

#include <gtest/gtest.h>

namespace
{
MeshID add_test_mesh(const std::initializer_list<glm::vec3> positions, std::vector<uint32_t> indices)
{
    ColorVertices vertices;
    vertices.reserve(positions.size());
    for (const auto& position : positions)
        vertices.push_back({ .pos = position });
    return MeshSystem::add(std::make_unique<ColorMesh>(std::move(vertices), std::move(indices)));
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

TEST(collider_tests, mesh_collider_rejects_aabb_hits_outside_the_mesh_triangles)
{
    const MeshID mesh_id = add_test_mesh({
        { -1.0f, -1.0f, 0.0f }, { 1.0f, -1.0f, 0.0f }, { -1.0f, 1.0f, 0.0f }
    }, { 0, 1, 2 });
    const MeshCollider mesh({ mesh_id });
    const RayCollider hit_ray(Maths::Ray(glm::vec3(-0.5f, -0.5f, -1.0f), Maths::forward_vec));
    const RayCollider gap_ray(Maths::Ray(glm::vec3(0.75f, 0.75f, -1.0f), Maths::forward_vec));
    glm::vec3 intersection;

    ASSERT_TRUE(mesh.check_collision(hit_ray, intersection));
    ASSERT_TRUE(glm_equal(intersection, glm::vec3(-0.5f, -0.5f, 0.0f)));
    ASSERT_FALSE(mesh.check_collision(gap_ray, intersection));
    MeshSystem::unregister_owner(mesh_id);
}

TEST(collider_tests, mesh_collider_returns_the_closest_triangle_across_meshes_and_transforms)
{
    const MeshID near_mesh = add_test_mesh({
        { -1.0f, -1.0f, 0.0f }, { 1.0f, -1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }
    }, { 0, 1, 2 });
    const MeshID far_mesh = add_test_mesh({
        { -1.0f, -1.0f, 1.0f }, { 1.0f, -1.0f, 1.0f }, { 0.0f, 1.0f, 1.0f }
    }, { 0, 1, 2 });
    MeshCollider mesh({ near_mesh, far_mesh });
    mesh.set_temporary_transform(Maths::Transform(
        glm::vec3(2.0f, 0.0f, 0.0f), glm::vec3(2.0f, 1.0f, 1.0f), Maths::identity_quat));
    const RayCollider ray(Maths::Ray(glm::vec3(2.0f, 0.0f, -1.0f), Maths::forward_vec));
    glm::vec3 intersection;

    ASSERT_TRUE(mesh.check_collision(ray, intersection));
    ASSERT_TRUE(glm_equal(intersection, glm::vec3(2.0f, 0.0f, 0.0f)));
    MeshSystem::unregister_owner(near_mesh);
    MeshSystem::unregister_owner(far_mesh);
}

TEST(collider_tests, mesh_collider_traverses_the_bvh_for_multi_triangle_meshes)
{
    const MeshID cube_id = MeshFactory::cube_id();
    const auto& pick_data = MeshSystem::get(cube_id).get_pick_data();
    ASSERT_GT(pick_data.get_nodes().size(), 1u);

    const MeshCollider mesh({ cube_id });
    const RayCollider ray(Maths::Ray(glm::vec3(0.0f, 0.0f, -2.0f), Maths::forward_vec));
    glm::vec3 intersection;
    ASSERT_TRUE(mesh.check_collision(ray, intersection));
    ASSERT_TRUE(glm_equal(intersection, glm::vec3(0.0f, 0.0f, -0.5f)));
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
