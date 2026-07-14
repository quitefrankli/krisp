#include "test_helper.hpp"

#include <camera.hpp>

#include <gtest/gtest.h>
#include <glm/gtx/string_cast.hpp>
#include <fmt/core.h>


class CameraTests : public testing::Test
{
public:
    CameraTests() : camera(Listener(), 1.0f)
    {
		camera.look_at(Maths::zero_vec, {0.0f, 0.0f, -2.0f});
		camera.update_tracker();
    }

	Camera camera;
};

TEST_F(CameraTests, lookat)
{
	glm::vec3 object = { 1.0f, 0.0f, 0.0f };
	camera.look_at(Maths::zero_vec, { 0.0f, 0.0f, -2.0f });
	glm::vec3 apparent_object_location = glm::vec3(camera.get_view() * glm::vec4(object, 1.0f));
	ASSERT_TRUE(glm_equal(apparent_object_location, {1.0f, 0.0f, 2.0f}));

	camera.look_at(object, { 0.0f, 0.0f, -2.0f });
	apparent_object_location = glm::vec3(camera.get_view() * glm::vec4(object, 1.0f));
	ASSERT_TRUE(glm_equal(apparent_object_location, {0.0f, 0.0f, Maths::sqrtf(5.0f)}));

	object = { 1.0f, 1.0f, 0.0f };
	camera.look_at({1.0f, 0.0f, -1.0f}, {-1.0f, 0.0f, 1.0f});
	apparent_object_location = glm::vec3(camera.get_view() * glm::vec4(object, 1.0f));
	ASSERT_TRUE(glm_equal(apparent_object_location, {-Maths::sqrtf(2.0f)/2.0f, 1.0f, 1.5f * Maths::sqrtf(2.0f)}));
}

TEST_F(CameraTests, camera_panning)
{
	// -1.0f, 1.0f == top-left corner
	// 1.0f, -1.0f == bot-right corner
	glm::vec2 start = { 0.0f, 0.0f };
	glm::vec2 end = { 1.0f, 0.0f };
	glm::vec2 delta = end - start;
	
	glm::vec3 object = Maths::zero_vec;
	glm::vec3 object2 = { 1.0f, 1.0f, 1.0f };
	glm::vec3 apparent_object = camera.get_view() * glm::vec4(object, 1.0f);
	ASSERT_TRUE(glm_equal(apparent_object, {0.0f, 0.0f, 2.0f}));

	camera.pan(delta, glm::length(delta));
	camera.update_tracker();
	apparent_object = camera.get_view() * glm::vec4(object, 1.0f);
	// Camera and focus translate together, preserving their relative transform.
	ASSERT_TRUE(glm_equal(apparent_object, {-0.4f, 0.0f, 2.0f}));

	delta = glm::vec2{ 0.0f, 0.5f } - start;
	camera.pan(delta, glm::length(delta));
	camera.update_tracker();
	apparent_object = camera.get_view() * glm::vec4(object, 1.0f);
	ASSERT_TRUE(glm_equal(apparent_object, {-0.4f, -0.2f, 2.0f}));
	apparent_object = camera.get_view() * glm::vec4(object2, 1.0f);
	ASSERT_TRUE(glm_equal(apparent_object, {0.6f, 0.8f, 3.0f}));
}

TEST_F(CameraTests, panning_after_orbit_preserves_view_direction)
{
	camera.rotate_camera({ 0.03f, -0.02f }, 0.1f);
	const glm::vec3 direction_before_pan = glm::normalize(camera.get_focus() - camera.get_position());
	const float focal_length_before_pan = camera.get_focal_length();

	// Match keyboard panning by moving along a camera-relative axis.
	const glm::vec3 keyboard_pan = camera.get_rotation() * Maths::right_vec;
	camera.pan(keyboard_pan, 0.5f);

	const glm::vec3 direction_after_pan = glm::normalize(camera.get_focus() - camera.get_position());
	EXPECT_TRUE(glm_equal(direction_after_pan, direction_before_pan));
	EXPECT_NEAR(camera.get_focal_length(), focal_length_before_pan, 0.001f);
}

TEST_F(CameraTests, camera_ray_cast)
{
	auto ray = camera.get_ray({ 0.0f, 0.0f });
	ASSERT_TRUE(glm_equal(ray.origin, { 0.0f, 0.0f, -2.0f }));
	ASSERT_TRUE(glm_equal(ray.direction, Maths::forward_vec));

	ray = camera.get_ray( { -1.0f, 0.0f });
	ASSERT_TRUE(glm_equal(ray.origin, { 0.0f, 0.0f, -2.0f }));
	ASSERT_TRUE(glm_equal(ray.direction, { -0.382683f, 0.000000f, 0.923880f }));

	ray = camera.get_ray( { 1.0f, 0.0f });
	ASSERT_TRUE(glm_equal(ray.origin, { 0.0f, 0.0f, -2.0f }));
	ASSERT_TRUE(glm_equal(ray.direction, { 0.382683f, 0.000000f, 0.923880f }));

	ray = camera.get_ray( { 0.0f, -1.0f });
	ASSERT_TRUE(glm_equal(ray.origin, { 0.0f, 0.0f, -2.0f }));
	ASSERT_TRUE(glm_equal(ray.direction, { 0.000000f, -0.382683f, 0.923880f }));

	ray = camera.get_ray( { 0.0f, 1.0f });
	ASSERT_TRUE(glm_equal(ray.origin, { 0.0f, 0.0f, -2.0f }));
	ASSERT_TRUE(glm_equal(ray.direction, { 0.000000f, 0.382683f, 0.923880f }));
}

TEST_F(CameraTests, reset_roll_and_focal_length_preserves_view_direction)
{
	const glm::vec3 direction_before_reset = glm::normalize(camera.get_focus() - camera.get_position());
	camera.focus_obj->set_rotation(glm::angleAxis(glm::half_pi<float>(), Maths::forward_vec) * camera.focus_obj->get_rotation());
	camera.zoom_in(-1.0f);
	ASSERT_NEAR(camera.get_focal_length(), 2.4f, 0.001f);

	camera.reset_roll_and_focal_length();
	ASSERT_NEAR(camera.get_focal_length(), 2.0f, 0.001f);
	ASSERT_TRUE(glm_equal(glm::normalize(camera.get_focus() - camera.get_position()), direction_before_reset));
	ASSERT_TRUE(glm_equal(camera.focus_obj->get_rotation() * Maths::up_vec, Maths::up_vec));
}

TEST_F(CameraTests, orthographic_camera_rays_start_on_the_corresponding_near_plane_point)
{
	camera.toggle_projection();

	const auto centre_ray = camera.get_ray({ 0.0f, 0.0f });
	ASSERT_TRUE(glm_equal(centre_ray.origin, { 0.0f, 0.0f, -1.9f }));
	ASSERT_TRUE(glm_equal(centre_ray.direction, Maths::forward_vec));

	const auto right_ray = camera.get_ray({ 1.0f, 0.0f });
	ASSERT_TRUE(glm_equal(right_ray.origin, { 1.0f, 0.0f, -1.9f }));
	ASSERT_TRUE(glm_equal(right_ray.direction, Maths::forward_vec));
}
