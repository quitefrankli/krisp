#include "../benchmark_scene.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

#include <array>
#include <cmath>


TEST(BenchmarkScene, cube_layout_is_a_stable_unique_prefix)
{
	std::array<bool, BenchmarkScene::lattice_cell_count> occupied{};
	for (int i = 0; i < BenchmarkScene::maximum_cube_count; ++i)
	{
		const glm::vec3 position = BenchmarkScene::cube_position(i);
		const int column = static_cast<int>(std::lround(position.x + 15.5f));
		const int row = static_cast<int>(std::lround(position.z + 15.5f));
		ASSERT_GE(column, 0);
		ASSERT_LT(column, 32);
		ASSERT_GE(row, 0);
		ASSERT_LT(row, 32);
		EXPECT_FLOAT_EQ(position.y, 0.3f);
		const int cell = row * 32 + column;
		EXPECT_FALSE(occupied[cell]);
		occupied[cell] = true;
	}
	EXPECT_THROW(BenchmarkScene::cube_position(-1), std::out_of_range);
	EXPECT_THROW(BenchmarkScene::cube_position(BenchmarkScene::lattice_cell_count), std::out_of_range);
}

TEST(BenchmarkScene, rotation_is_a_pure_function_of_tick)
{
	const glm::quat expected = BenchmarkScene::rotation_for_tick(900);
	EXPECT_EQ(BenchmarkScene::rotation_for_tick(0), glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
	(void)BenchmarkScene::rotation_for_tick(42);
	(void)BenchmarkScene::rotation_for_tick(1'234);
	const glm::quat repeated = BenchmarkScene::rotation_for_tick(900);
	EXPECT_FLOAT_EQ(repeated.w, expected.w);
	EXPECT_FLOAT_EQ(repeated.x, expected.x);
	EXPECT_FLOAT_EQ(repeated.y, expected.y);
	EXPECT_FLOAT_EQ(repeated.z, expected.z);
}

TEST(BenchmarkScene, configured_camera_contains_every_cube_corner)
{
	const glm::mat4 view = glm::lookAtLH(glm::vec3(0.0f, 32.0f, -32.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	const glm::mat4 projection = glm::perspectiveLH(glm::radians(45.0f), 1280.0f / 720.0f, 0.1f, 250.0f);
	EXPECT_TRUE(BenchmarkScene::cube_grid_in_frustum(projection * view));
}
