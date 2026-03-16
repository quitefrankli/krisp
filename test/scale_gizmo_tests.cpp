#include "test_helper.hpp"

#include <maths.hpp>

#include <gtest/gtest.h>
#include <glm/gtx/component_wise.hpp>


// helper: replicates the uniform scale math from ScaleGizmo::process
static float compute_uniform_magnitude(const glm::vec3& view_dir, const glm::vec3& p1, const glm::vec3& p2)
{
	const glm::vec3 plane_normal = glm::normalize(-view_dir);
	const glm::vec3 cam_right = glm::normalize(glm::cross(-plane_normal, Maths::up_vec));
	return -glm::dot(p2 - p1, cam_right);
}

TEST(ScaleGizmoTests, drag_left_expands_and_drag_right_shrinks)
{
	const glm::vec3 view_dir(0.0f, 0.0f, -1.0f);
	const glm::vec3 origin(0.0f, 0.0f, 0.0f);

	// drag left on screen -> scale up
	ASSERT_GT(compute_uniform_magnitude(view_dir, origin, {-0.5f, 0.0f, 0.0f}), 0.0f);
	// drag right on screen -> scale down
	ASSERT_LT(compute_uniform_magnitude(view_dir, origin, {0.5f, 0.0f, 0.0f}), 0.0f);

	// from a camera looking along -X, screen right is -Z
	const glm::vec3 side_view(-1.0f, 0.0f, 0.0f);
	ASSERT_GT(compute_uniform_magnitude(side_view, origin, {0.0f, 0.0f, 0.5f}), 0.0f);
	ASSERT_LT(compute_uniform_magnitude(side_view, origin, {0.0f, 0.0f, -0.5f}), 0.0f);
}

TEST(ScaleGizmoTests, centre_cube_collision_detection)
{
	constexpr float CUBE_RADIUS = 0.15f;
	const glm::vec3 cube_pos(0.0f, 0.0f, 0.0f);

	// ray that hits the centre cube
	Maths::Ray hit_ray;
	hit_ray.origin = glm::vec3(0.0f, 0.0f, 5.0f);
	hit_ray.direction = glm::vec3(0.0f, 0.0f, -1.0f);
	ASSERT_TRUE(Maths::check_spherical_collision(hit_ray, Maths::Sphere(cube_pos, CUBE_RADIUS)));

	// ray that misses the centre cube
	Maths::Ray miss_ray;
	miss_ray.origin = glm::vec3(1.0f, 0.0f, 5.0f);
	miss_ray.direction = glm::vec3(0.0f, 0.0f, -1.0f);
	ASSERT_FALSE(Maths::check_spherical_collision(miss_ray, Maths::Sphere(cube_pos, CUBE_RADIUS)));
}
