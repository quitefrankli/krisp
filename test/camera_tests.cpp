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
	ASSERT_TRUE(glm_equal(apparent_object_location, {0.0f, 0.0f, std::sqrtf(5.0f)}));

	object = { 1.0f, 1.0f, 0.0f };
	camera.look_at({1.0f, 0.0f, -1.0f}, {-1.0f, 0.0f, 1.0f});
	apparent_object_location = glm::vec3(camera.get_view() * glm::vec4(object, 1.0f));
	ASSERT_TRUE(glm_equal(apparent_object_location, {-std::sqrtf(2.0f)/2.0f, 1.0f, 1.5f * std::sqrtf(2.0f)}));
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
	// 0.4f from Camera::panning_sensitivity * camera.focal_length()
	ASSERT_TRUE(glm_equal(apparent_object, {-0.4f, 0.0f, 2.0f}));

	delta = glm::vec2{ 0.0f, 0.5f } - start;
	camera.pan(delta, glm::length(delta));
	camera.update_tracker();
	apparent_object = camera.get_view() * glm::vec4(object, 1.0f);
	ASSERT_TRUE(glm_equal(apparent_object, {-0.4f, -0.2f, 2.0f}));
	apparent_object = camera.get_view() * glm::vec4(object2, 1.0f);
	ASSERT_TRUE(glm_equal(apparent_object, {0.6f, 0.8f, 3.0f}));
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