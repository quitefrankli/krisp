#include "test_helper.hpp"

#include <collision/collider.hpp>
#include <collision/collision_detector.hpp>

#include <gtest/gtest.h>


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