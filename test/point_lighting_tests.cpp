#include <graphics_engine/point_lighting.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>


namespace
{
bool matrices_differ(const glm::mat4& lhs, const glm::mat4& rhs)
{
	for (glm::length_t column = 0; column < 4; ++column)
		for (glm::length_t row = 0; row < 4; ++row)
			if (std::abs(lhs[column][row] - rhs[column][row]) > 0.00001f)
				return true;
	return false;
}
}


TEST(PointLighting, assigns_dense_shadow_slots_only_to_enabled_radiant_shadow_lights)
{
	const std::vector<RenderLightState> lights{
		{ .object_id = ObjectID(1), .position = { 1.0f, 2.0f, 3.0f } },
		{ .object_id = ObjectID(2), .enabled = false },
		{ .object_id = ObjectID(3), .intensity = 0.0f },
		{ .object_id = ObjectID(4), .color = glm::vec3(0.0f) },
		{ .object_id = ObjectID(5), .casts_shadow = false },
		{ .object_id = ObjectID(6), .position = { 4.0f, 5.0f, 6.0f } },
	};

	const auto prepared = prepare_point_lighting(lights);
	EXPECT_EQ(prepared.light_count, 6);
	EXPECT_EQ(prepared.shadow_count, 2);
	EXPECT_EQ(prepared.lights[0].shadow_slot, 0);
	EXPECT_EQ(prepared.lights[1].shadow_slot, -1);
	EXPECT_EQ(prepared.lights[2].shadow_slot, -1);
	EXPECT_EQ(prepared.lights[3].shadow_slot, -1);
	EXPECT_EQ(prepared.lights[4].shadow_slot, -1);
	EXPECT_EQ(prepared.lights[5].shadow_slot, 1);
	EXPECT_FLOAT_EQ(prepared.lights[1].intensity, 0.0f);
	EXPECT_EQ(prepared.lights[0].position, lights[0].position);
	EXPECT_EQ(prepared.lights[5].position, lights[5].position);
}

TEST(PointLighting, builds_shadow_projections_from_each_light_position)
{
	const std::vector<RenderLightState> lights{
		{ .object_id = ObjectID(1), .position = { 1.0f, 2.0f, 3.0f } },
		{ .object_id = ObjectID(2), .position = { -4.0f, 5.0f, 6.0f } },
	};

	const auto prepared = prepare_point_lighting(lights);
	ASSERT_EQ(prepared.shadow_count, 2);
	for (std::size_t face = 0; face < 6; ++face)
		EXPECT_TRUE(matrices_differ(
			prepared.lights[0].shadow_view_proj_mats[face],
			prepared.lights[1].shadow_view_proj_mats[face]));
}

TEST(PointLighting, rejects_capacity_overflow_and_invalid_light_data)
{
	std::vector<RenderLightState> too_many(SDS::MAX_POINT_LIGHTS + 1);
	EXPECT_THROW(prepare_point_lighting(too_many), std::invalid_argument);

	EXPECT_THROW(prepare_point_lighting(std::vector<RenderLightState>{
		{ .object_id = ObjectID(1), .intensity = -1.0f },
	}), std::invalid_argument);
	EXPECT_THROW(prepare_point_lighting(std::vector<RenderLightState>{
		{ .object_id = ObjectID(1), .color = { 1.0f, std::numeric_limits<float>::quiet_NaN(), 1.0f } },
	}), std::invalid_argument);
	EXPECT_THROW(prepare_point_lighting(std::vector<RenderLightState>{
		{ .object_id = ObjectID(1), .position = { std::numeric_limits<float>::infinity(), 0.0f, 0.0f } },
	}), std::invalid_argument);
}

TEST(PointLighting, accepts_zero_lights)
{
	const std::vector<RenderLightState> lights;
	const auto prepared = prepare_point_lighting(lights);
	EXPECT_EQ(prepared.light_count, 0);
	EXPECT_EQ(prepared.shadow_count, 0);
}
