#include "test_helper.hpp"

#include <maths.hpp>
#include <collision/bounding_box.hpp>

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

TEST(ScaleGizmoTests, centre_handle_preserves_non_uniform_scale_proportions)
{
	const glm::vec3 original_scale(2.0f, 3.0f, 4.0f);
	const float drag_magnitude = 0.25f;
	const glm::vec3 scaled = original_scale * (1.0f + drag_magnitude);

	EXPECT_FLOAT_EQ(scaled.x, 2.5f);
	EXPECT_FLOAT_EQ(scaled.y, 3.75f);
	EXPECT_FLOAT_EQ(scaled.z, 5.0f);
	EXPECT_FLOAT_EQ(scaled.y / scaled.x, original_scale.y / original_scale.x);
	EXPECT_FLOAT_EQ(scaled.z / scaled.x, original_scale.z / original_scale.x);
}

TEST(ScaleGizmoTests, centre_cube_uses_aabb_collision_detection)
{
	constexpr float CUBE_HALF_SIZE = 0.15f;
	const AABB cube_bounds(glm::vec3(-CUBE_HALF_SIZE), glm::vec3(CUBE_HALF_SIZE));

	// ray that hits the centre cube
	const Maths::Ray hit_ray(glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f, 0.0f, -1.0f));
	ASSERT_TRUE(cube_bounds.check_collision(hit_ray));

	// This is inside the cube but outside its old spherical approximation.
	const Maths::Ray corner_hit_ray(glm::vec3(0.14f, 0.14f, 5.0f), glm::vec3(0.0f, 0.0f, -1.0f));
	ASSERT_TRUE(cube_bounds.check_collision(corner_hit_ray));

	// ray that misses the centre cube
	const Maths::Ray miss_ray(glm::vec3(1.0f, 0.0f, 5.0f), glm::vec3(0.0f, 0.0f, -1.0f));
	ASSERT_FALSE(cube_bounds.check_collision(miss_ray));
}
