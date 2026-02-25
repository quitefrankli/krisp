#include <entity_component_system/ecs.hpp>

#include <gtest/gtest.h>

#include <memory>


class TileSystemFixture : public testing::Test
{
public:
	ECS ecs;
	Object obj1, obj2;
	std::vector<std::unique_ptr<Object>> spawned_tiles;

	TileSystemFixture()
	{
		ecs.add_object(obj1);
		ecs.add_object(obj2);
	}

	void set_tile_spawner(glm::vec3 initial_position = glm::vec3(0.0f),
	                      glm::vec3 initial_scale = glm::vec3(1.0f))
	{
		ecs.set_tile_spawner([this, initial_position, initial_scale]() -> Object&
		{
			auto tile = std::make_unique<Object>();
			tile->set_position(initial_position);
			tile->set_scale(initial_scale);
			Object& ref = *tile;
			ecs.add_object(ref);
			spawned_tiles.push_back(std::move(tile));
			return ref;
		});
	}
};

TEST_F(TileSystemFixture, move_and_retrieve_objects)
{
	ecs.spawn_tileset(8, 8, 1.0f);
	ecs.move_to_tile({0, 0}, obj1.get_id());
	ecs.move_to_tile({1, 2}, obj2.get_id());

	auto* tile1 = ecs.get_tile({0, 0});
	ASSERT_NE(tile1, nullptr);
	ASSERT_FALSE(tile1->is_empty());
	ASSERT_EQ(tile1->get_objects().size(), 1);
	ASSERT_EQ(tile1->get_objects()[0], obj1.get_id());

	auto* tile2 = ecs.get_tile({1, 2});
	ASSERT_NE(tile2, nullptr);
	ASSERT_EQ(tile2->get_objects()[0], obj2.get_id());

	ASSERT_EQ(ecs.get_tile({5, 5}), nullptr);
}

TEST_F(TileSystemFixture, move_to_tile_updates_position)
{
	constexpr float cell_size = 5.0f;
	ecs.spawn_tileset(8, 8, cell_size);

	ecs.move_to_tile({3, 4}, obj1.get_id());
	EXPECT_FLOAT_EQ(obj1.get_position().x, 3 * cell_size);
	EXPECT_FLOAT_EQ(obj1.get_position().z, 4 * cell_size);

	ecs.move_to_tile({7, 2}, obj1.get_id());
	EXPECT_FLOAT_EQ(obj1.get_position().x, 7 * cell_size);
	EXPECT_FLOAT_EQ(obj1.get_position().z, 2 * cell_size);
}

TEST_F(TileSystemFixture, remove_entity_clears_from_all_tiles)
{
	ecs.spawn_tileset(4, 4, 1.0f);
	ecs.move_to_tile({0, 0}, obj1.get_id());
	ecs.move_to_tile({1, 1}, obj2.get_id());

	ASSERT_FALSE(ecs.get_tile({0, 0})->is_empty());
	ASSERT_FALSE(ecs.get_tile({1, 1})->is_empty());

	ecs.TileSystem::remove_entity(obj1.get_id());

	EXPECT_TRUE(ecs.get_tile({0, 0})->is_empty());
	EXPECT_FALSE(ecs.get_tile({1, 1})->is_empty());
}

TEST_F(TileSystemFixture, create_grid_scales_spawned_cube_to_tile_and_aligns_top_surface)
{
	set_tile_spawner();

	constexpr float cell_size = 2.5f;
	constexpr float tile_gap = 0.01f;
	constexpr float tile_thickness = 0.05f;
	ecs.spawn_tileset(1, 1, cell_size);

	ASSERT_EQ(spawned_tiles.size(), 1);
	const auto scale = spawned_tiles[0]->get_scale();
	const auto pos = spawned_tiles[0]->get_position();

	EXPECT_FLOAT_EQ(scale.x, cell_size - tile_gap);
	EXPECT_FLOAT_EQ(scale.y, tile_thickness);
	EXPECT_FLOAT_EQ(scale.z, cell_size - tile_gap);
	EXPECT_FLOAT_EQ(pos.x, 0.0f);
	EXPECT_FLOAT_EQ(pos.y + scale.y * 0.5f, 0.0f);
	EXPECT_FLOAT_EQ(pos.z, 0.0f);
}

TEST_F(TileSystemFixture, create_grid_centers_odd_sized_tileset_and_keeps_no_gaps)
{
	set_tile_spawner();

	constexpr float cell_size = 2.0f;
	ecs.spawn_tileset(3, 3, cell_size);

	ASSERT_EQ(spawned_tiles.size(), 9);
	const auto& left = *spawned_tiles[3];
	const auto& center = *spawned_tiles[4];
	const auto& right = *spawned_tiles[5];
	const auto& top = *spawned_tiles[1];
	const auto& bottom = *spawned_tiles[7];

	EXPECT_FLOAT_EQ(center.get_position().x, 0.0f);
	EXPECT_FLOAT_EQ(center.get_position().z, 0.0f);
	EXPECT_FLOAT_EQ(center.get_position().y + center.get_scale().y * 0.5f, 0.0f);

	EXPECT_FLOAT_EQ(center.get_position().x - left.get_position().x, cell_size);
	EXPECT_FLOAT_EQ(right.get_position().x - center.get_position().x, cell_size);
	EXPECT_FLOAT_EQ(center.get_position().z - top.get_position().z, cell_size);
	EXPECT_FLOAT_EQ(bottom.get_position().z - center.get_position().z, cell_size);
}

TEST_F(TileSystemFixture, create_grid_centers_even_rows_and_cols_and_overrides_initial_height_offset)
{
	set_tile_spawner(glm::vec3(13.0f, 7.0f, -9.0f), glm::vec3(1.0f, 0.2f, 1.0f));

	constexpr float cell_size = 3.0f;
	constexpr float tile_gap = 0.01f;
	ecs.spawn_tileset(2, 4, cell_size);

	ASSERT_EQ(spawned_tiles.size(), 8);
	const auto& first = *spawned_tiles[0];   // row 0, col 0
	const auto& second = *spawned_tiles[1];  // row 0, col 1
	const auto& last = *spawned_tiles[7];    // row 1, col 3

	EXPECT_FLOAT_EQ(first.get_scale().x, cell_size - tile_gap);
	EXPECT_FLOAT_EQ(first.get_scale().z, cell_size - tile_gap);
	EXPECT_FLOAT_EQ(first.get_position().x, -4.5f);
	EXPECT_FLOAT_EQ(first.get_position().z, -1.5f);
	EXPECT_FLOAT_EQ(last.get_position().x, 4.5f);
	EXPECT_FLOAT_EQ(last.get_position().z, 1.5f);
	EXPECT_FLOAT_EQ(first.get_position().y + first.get_scale().y * 0.5f, 0.0f);

	EXPECT_FLOAT_EQ(second.get_position().x - first.get_position().x, cell_size);
}
