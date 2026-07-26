#include <maths.hpp>

#include <gtest/gtest.h>

#include <iostream>
#include <glm/gtx/string_cast.hpp>


TEST(math_tests, math_constants)
{
    ASSERT_EQ(Maths::forward_vec, glm::vec3(0.0f, 0.0f, 1.0f));
    ASSERT_EQ(Maths::right_vec, glm::vec3(1.0f, 0.0f, 0.0f));
    ASSERT_EQ(Maths::up_vec, glm::vec3(0.0f, 1.0f, 0.0f));
}

TEST(math_tests, ray_plane_intersections)
{
    const Maths::Plane plane(glm::vec3(0.0f, 0.0f, 0.0f), Maths::forward_vec);
    Maths::Ray ray(glm::vec3(0.0f, 0.0f, -1.0f), Maths::forward_vec);
    ASSERT_EQ(Maths::check_ray_plane_intersection(ray, plane), true);
    glm::vec3 intersection = Maths::ray_plane_intersection(ray, plane);
    ASSERT_TRUE(Maths::is_vec3_equal(intersection, glm::vec3(0.0f)));

    ray.direction = glm::normalize(glm::vec3(1.0f, 0.0f, 1.0f));
    ASSERT_EQ(Maths::check_ray_plane_intersection(ray, plane), true);
    intersection = Maths::ray_plane_intersection(ray, plane);
    ASSERT_TRUE(Maths::is_vec3_equal(intersection, glm::vec3(1.0f, 0.0f, 0.0f)));
}

TEST(math_tests, ray_plane_intersections_plane_behind_ray)
{
    Maths::Plane plane(glm::vec3(0.0f, 0.0f, 0.0f), Maths::forward_vec);
    Maths::Ray ray(glm::vec3(0.0f, 0.0f, 1.0f), Maths::forward_vec);
    ASSERT_EQ(Maths::check_ray_plane_intersection(ray, plane), false);

    ray.direction = glm::normalize(glm::vec3(1.0f, 0.0f, 1.0f));
    ASSERT_EQ(Maths::check_ray_plane_intersection(ray, plane), false);
}

TEST(math_tests, ray_starting_on_plane_intersects_at_its_origin)
{
    const Maths::Plane plane(glm::vec3(0.0f), Maths::forward_vec);
    const Maths::Ray ray(glm::vec3(0.0f, 1.0f, 0.0f), Maths::forward_vec);
    ASSERT_TRUE(Maths::check_ray_plane_intersection(ray, plane));
    ASSERT_TRUE(Maths::is_vec3_equal(Maths::ray_plane_intersection(ray, plane), ray.origin));
}

TEST(math_tests, ray_sphere_intersection_rejects_misses_and_accepts_non_normalized_rays)
{
    const Maths::Sphere sphere(Maths::zero_vec, 0.5f);
    ASSERT_FALSE(Maths::ray_sphere_collision(
        sphere, Maths::Ray(glm::vec3(1.0f, 0.0f, -1.0f), Maths::forward_vec)).has_value());

    const auto hit = Maths::ray_sphere_collision(
        sphere, Maths::Ray(glm::vec3(0.0f, 0.0f, -2.0f), 2.0f * Maths::forward_vec));
    ASSERT_TRUE(hit.has_value());
    ASSERT_TRUE(Maths::is_vec3_equal(*hit, glm::vec3(0.0f, 0.0f, -0.5f)));
}

TEST(math_tests, ray_plane_intersections_plane_orthogonal_to_ray)
{
    // 1. if origin is different then expect no intersect
    Maths::Plane plane(glm::vec3(0.0f, 0.0f, 0.0f), Maths::right_vec);
    Maths::Ray ray(glm::vec3(1.0f, 0.0f, 0.0f), Maths::forward_vec);
    ASSERT_EQ(Maths::check_ray_plane_intersection(ray, plane), false);

    // 2. if origin is same then expect permanent intersect
    ray.origin = plane.offset;
    ASSERT_EQ(Maths::check_ray_plane_intersection(ray, plane), true);
}

TEST(math_tests, updating_component_of_matrix_transform_preserves_other_components)
{
    const glm::vec3 original_scale(2.0f, 3.0f, 4.0f);
    const glm::quat original_orientation =
        glm::angleAxis(glm::radians(30.0f), glm::normalize(glm::vec3(1.0f, 2.0f, 3.0f)));
    const glm::mat4 original =
        glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 2.0f, 3.0f)) *
        glm::mat4_cast(original_orientation) *
        glm::scale(glm::mat4(1.0f), original_scale);

    Maths::Transform transform(original);
    transform.set_pos(glm::vec3(4.0f, 5.0f, 6.0f));

    const glm::mat4& updated = transform.get_mat4();
    EXPECT_TRUE(Maths::is_vec3_equal(glm::vec3(updated[3]), glm::vec3(4.0f, 5.0f, 6.0f)));
    EXPECT_TRUE(Maths::is_vec3_equal(transform.get_scale(), original_scale));
    EXPECT_NEAR(glm::abs(glm::dot(transform.get_orient(), original_orientation)), 1.0f, 0.0001f);
}
