#include <maths.hpp>

#include <gtest/gtest.h>


TEST(math_constants)
{
    ASSERT_EQ(Maths::forward_vec, glm::vec3(0.0f, 0.0f, 1.0f));
    ASSERT_EQ(Maths::right_vec, glm::vec3(1.0f, 0.0f, 0.0f));
    ASSERT_EQ(Maths::up_vec, glm::vec3(0.0f, 1.0f, 0.0f));
}

TEST(ray_plane_intersections)
{
    Maths::Plane plane(glm::vec3(0.0f, 0.0f, 0.0f), Maths::forward_vec);
    Maths::Ray ray(glm::vec3(0.0f, -1.0f, 0.0f), Maths::forward_vec);
    ASSERT_EQ(Maths::check_ray_plane_intersection(ray, plane), true);
    glm::vec3 intersection = Maths::ray_plane_intersection(ray, plane);
    ASSERT_EQ(intersection, glm::vec3(0.0f));

    ray.direction = glm::normalize(glm::vec3(1.0f, 1.0f, 0.0f));
    ASSERT_EQ(Maths::check_ray_plane_intersection(ray, plane), true);
    intersection = Maths::ray_plane_intersection(ray, plane);
    ASSERT_EQ(intersection, glm::vec3(1.0f, 0.0f, 0.0f));
}

TEST(ray_plane_intersections_plane_behind_ray)
{
    Maths::Plane plane(glm::vec3(0.0f, 0.0f, 0.0f), Maths::forward_vec);
    Maths::Ray ray(glm::vec3(0.0f, 1.0f, 0.0f), Maths::forward_vec);
    ASSERT_EQ(Maths::check_ray_plane_intersection(ray, plane), false);

    ray.direction = glm::normalize(glm::vec3(1.0f, 1.0f, 0.0f));
    ASSERT_EQ(Maths::check_ray_plane_intersection(ray, plane), false);
}

TEST(ray_plane_intersections_plane_orthogonal_to_ray)
{
    // 1. if origin is different then expect no intersect
    Maths::Plane plane(glm::vec3(0.0f, 0.0f, 0.0f), Maths::right_vec);
    Maths::Ray ray(glm::vec3(0.0f, -1.0f, 0.0f), Maths::forward_vec);
    ASSERT_EQ(Maths::check_ray_plane_intersection(ray, plane), true);
    glm::vec3 intersection = Maths::ray_plane_intersection(ray, plane);
    ASSERT_EQ(intersection, glm::vec3(0.0f));

    // 2. if origin is same then expect permanent intersect
    ray.direction = glm::normalize(glm::vec3(1.0f, 1.0f, 0.0f));
    ASSERT_EQ(Maths::check_ray_plane_intersection(ray, plane), true);
    intersection = Maths::ray_plane_intersection(ray, plane);
    ASSERT_EQ(intersection, glm::vec3(1.0f, 0.0f, 0.0f));
}