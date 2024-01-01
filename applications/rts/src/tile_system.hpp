#pragma once

#include <objects/object_id.hpp>

#include <glm/vec2.hpp>

#include <vector>
#include <unordered_map>
#include <functional>


/*
	TileCoord is a coordinate in the tile system. It is a 2D vector of integers.
	The origin at (0, 0), down is (0, -1), left is (-1, 0), up is (0, 1), right is (1, 0).
*/
using TileCoord = glm::ivec2;
namespace std
{
	template<>
	struct hash<TileCoord>
	{
		std::size_t operator()(const TileCoord& coord) const noexcept
		{
			return std::hash<int>()(coord.x) ^ std::hash<int>()(coord.y);
		}
	};
}

class Tile
{
public:
	Tile(const TileCoord& coord);

	const TileCoord coord;

	bool is_empty() const { return objects.empty(); }

	void add_object(ObjectID id) { objects.push_back(id); }
	void remove_object(ObjectID id) { objects.erase(std::find(objects.begin(), objects.end(), id)); }
	std::vector<ObjectID> get_objects() const { return objects; }

	using TileObjectSpawner = std::function<void(const TileCoord& coord)>;
	static TileObjectSpawner tile_object_spawner;

private:
	std::vector<ObjectID> objects;
};

// This is useful in scenarios i.e. houses, different dimensions, different floors
class TileSet
{
public:
	using TileSetID = std::string;

	void add_tile(const TileCoord& coord);
	Tile* get_tile(const TileCoord& coord);
	std::vector<Tile*> get_tiles(const ObjectID& object_id);
	void remove_object(const ObjectID& object_id);

private:
	std::unordered_map<TileCoord, Tile> tiles;
	std::unordered_map<ObjectID, std::vector<Tile*>> object_to_tile;
};

class TileSystem
{
public:
	TileSystem();
	
	Tile* get_tile(const TileCoord& coord, const TileSet::TileSetID& tileset_id = {});
	std::vector<Tile*> get_tiles(const ObjectID& object_id, const TileSet::TileSetID& tileset_id = {});
	void remove_object(const ObjectID& object_id);

	void add_to_tile(const TileCoord& coord, const ObjectID& object_id, const TileSet::TileSetID& tileset_id = {});
	void remove_from_tile(const TileCoord& coord, const ObjectID& object_id, const TileSet::TileSetID& tileset_id = {});

private:
	std::unordered_map<TileSet::TileSetID, TileSet> tilesets;
};