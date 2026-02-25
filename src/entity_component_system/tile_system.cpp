#include "tile_system.hpp"
#include "ecs.hpp"


void TileSet::move_to_tile(const TileCoord& coord, Object& object)
{
	auto& tile = tiles[coord];
	tile.add_object(object.get_id());
	object_to_tile[object.get_id()] = &tile;

	object.set_position(glm::vec3(coord.x * cell_size, object.get_position().y, coord.y * cell_size));
}

void TileSet::remove_object(const ObjectID& object_id)
{
	const auto it = object_to_tile.find(object_id);
	if (it == object_to_tile.end())
		return;

	auto* tile = it->second;
	tile->remove_object(object_id);
	object_to_tile.erase(it);
}

glm::vec3 TileSet::get_tile_center(const TileCoord & coord) const
{
	const float half_cell = 0.5f * cell_size;
	const float thickness = 0.05f;
	return glm::vec3(coord.x * cell_size + half_cell, -0.5f * thickness, coord.y * cell_size + half_cell);
}

void TileSystem::spawn_tileset(int rows, int cols, float cell_size, const TileSetID& tileset_id)
{
	auto& tileset = tilesets[tileset_id] = TileSet{ .cell_size = cell_size };
	const float x_start = -0.5f * static_cast<float>(cols) * cell_size + 0.5f * cell_size;
	const float z_start = -0.5f * static_cast<float>(rows) * cell_size + 0.5f * cell_size;
	const float thickness = 0.05f;
	const float gap = 0.01f;
	for (int row = 0; row < rows; ++row)
	{
		for (int col = 0; col < cols; ++col)
		{
			if (tile_spawner)
			{
				auto& tile = tile_spawner();
				tile.set_scale(glm::vec3(cell_size - gap, thickness, cell_size - gap));

				const float x = x_start + static_cast<float>(col) * cell_size;
				const float z = z_start + static_cast<float>(row) * cell_size;
				// Unit cube mesh is centered on its origin, so move it down by half height to keep the top face at y=0.
				const float y = -0.5f * thickness;
				const auto position = glm::vec3(x, y, z);
				tile.set_position(position);

				const Maths::Plane plane(position, glm::vec3(0.0f, 1.0f, 0.0f));
				const glm::vec2 size(cell_size - gap, cell_size - gap);
				get_ecs().add_collider(tile.get_id(), std::make_unique<QuadCollider>(plane, size));
				get_ecs().add_hoverable_entity(tile.get_id());

				all_tiles.insert(tile.get_id());
			}
		}
	}
}

void TileSystem::remove_entity(const ObjectID& object_id)
{
	for (auto& [_, tileset] : tilesets)
		tileset.remove_object(object_id);
	all_tiles.erase(object_id);
	if (prev_hovered && *prev_hovered == object_id)
		prev_hovered.reset();
}

Tile* TileSystem::get_tile(const TileCoord& coord, const TileSetID& tileset_id)
{
	if (!tilesets.contains(tileset_id))
		return nullptr;

	auto& tiles = tilesets[tileset_id].tiles;
	if (!tiles.contains(coord))
		return nullptr;
	return &tiles[coord];
}

const Tile* TileSystem::get_tile(const TileCoord& coord, const TileSetID& tileset_id) const
{
	const auto tileset_it = tilesets.find(tileset_id);
	if (tileset_it == tilesets.end())
		return nullptr;

	const auto tile_it = tileset_it->second.tiles.find(coord);
	if (tile_it == tileset_it->second.tiles.end())
		return nullptr;
	return &tile_it->second;
}

void TileSystem::move_to_tile(const TileCoord& coord, const ObjectID& object_id, const TileSetID& tileset_id)
{
	if (!tilesets.contains(tileset_id))
	{
		throw std::runtime_error("TileSystem::move_to_tile: tileset_id not found!");
	}

	remove_entity(object_id);
	tilesets[tileset_id].move_to_tile(coord, get_ecs().get_object(object_id));
}


TileSystem::HoverResult TileSystem::process_hover(const Maths::Ray& ray)
{
	const auto hovered = get_ecs().check_any_entity_hovered(ray);

	HoverResult result;
	result.prev_hovered = prev_hovered;

	if (!hovered.bCollided || !all_tiles.contains(hovered.id))
	{
		return result;
	}

	if (prev_hovered && *prev_hovered == hovered.id)
	{
		return {};
	}

	result.new_hovered = hovered.id;
	prev_hovered = hovered.id;

	return result;
}
