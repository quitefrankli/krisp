#include "tile_system.hpp"


void TileSet::add_tile(const TileCoord& coord)
{
	tiles.emplace(coord, Tile(coord));
}

Tile* TileSet::get_tile(const TileCoord& coord)
{
	auto it = tiles.find(coord);
	if (it == tiles.end())
		return nullptr;
	return &it->second;
}

std::vector<Tile*> TileSet::get_tiles(const ObjectID& object_id)
{
	auto it = object_to_tile.find(object_id);
	if (it == object_to_tile.end())
		return {};
	return it->second;
}

void TileSet::remove_object(const ObjectID& object_id)
{
	auto it = object_to_tile.find(object_id);
	if (it == object_to_tile.end())
		return;

	for (auto tile : it->second)
	{
		tile->remove_object(object_id);
	}

	object_to_tile.erase(it);
}

TileSystem::TileSystem()
{
	auto& default_tileset = tilesets.emplace("", TileSet()).first->second;
	// create a default tileset with 21x21 tiles
	for (int i = -10; i <= 10; ++i)
	{
		for (int j = -10; j <= 10; ++j)
		{
			default_tileset.add_tile({i, j});
		}
	}
}

Tile* TileSystem::get_tile(const TileCoord& coord, const TileSet::TileSetID& tileset_id)
{
	auto it = tilesets.find(tileset_id);
	if (it == tilesets.end())
		return nullptr;
	return it->second.get_tile(coord);
}

std::vector<Tile*> TileSystem::get_tiles(const ObjectID& object_id, const TileSet::TileSetID& tileset_id)
{
	auto it = tilesets.find(tileset_id);
	if (it == tilesets.end())
		return {};
	return it->second.get_tiles(object_id);
}

void TileSystem::remove_object(const ObjectID& object_id)
{
	for (auto& [_, tileset] : tilesets)
	{
		tileset.remove_object(object_id);
	}
}

void TileSystem::add_to_tile(const TileCoord& coord, const ObjectID& object_id, const TileSet::TileSetID& tileset_id) 
{
	auto it = tilesets.find(tileset_id);
	if (it == tilesets.end())
	{
		tilesets.emplace(tileset_id, TileSet());
		it = tilesets.find(tileset_id);
	}

	it->second.add_tile(coord);
	it->second.get_tile(coord)->add_object(object_id);
	it->second.get_tiles(object_id).push_back(it->second.get_tile(coord));
}

void TileSystem::remove_from_tile(const TileCoord& coord,
                                  const ObjectID& object_id,
                                  const TileSet::TileSetID& tileset_id)
{
	auto it = tilesets.find(tileset_id);
	if (it == tilesets.end())
		return;

	it->second.get_tile(coord)->remove_object(object_id);
}

Tile::TileObjectSpawner Tile::tile_object_spawner = [](const TileCoord& coord)
{
};

Tile::Tile(const TileCoord& coord) :
	coord(coord)
{
	tile_object_spawner(coord);
}