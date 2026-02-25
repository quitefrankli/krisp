#pragma once

#include "identifications.hpp"
#include "maths.hpp"

#include <glm/vec2.hpp>

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <optional>


using TileCoord = glm::ivec2;
using TileSetID = std::string;

class Object;

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

struct Tile
{
	void add_object(const ObjectID& object_id)
	{
		objects.insert(object_id);
	}
	
	void remove_object(const ObjectID& object_id)
	{
		objects.erase(object_id);
	}

	bool is_empty() const
	{
		return objects.empty();
	}

	std::vector<ObjectID> get_objects() const
	{
		return {objects.begin(), objects.end()};
	}

	std::unordered_set<ObjectID> objects;
};

struct TileSet
{
	float cell_size = 1.0f;
	std::unordered_map<TileCoord, Tile> tiles;
	std::unordered_map<ObjectID, Tile*> object_to_tile;

	void move_to_tile(const TileCoord& coord, Object& object);
	void remove_object(const ObjectID& object_id);

	glm::vec3 get_tile_center(const TileCoord& coord) const;
};

using TileSets = std::unordered_map<TileSetID, TileSet>;

class ECS;

class TileSystem
{
public:
	using TileObjectSpawner = std::function<Object&()>;

	// called by game engine
	void set_tile_spawner(TileObjectSpawner spawner) { tile_spawner = std::move(spawner); }

	void spawn_tileset(int rows, int cols, float cell_size, const TileSetID& tileset_id = {});
	void move_to_tile(const TileCoord& coord, const ObjectID& object_id, const TileSetID& tileset_id = {});

	Tile* get_tile(const TileCoord& coord, const TileSetID& tileset_id = {});
	const Tile* get_tile(const TileCoord& coord, const TileSetID& tileset_id = {}) const;

	void remove_entity(const ObjectID& object_id);

	virtual ECS& get_ecs() = 0;
	virtual const ECS& get_ecs() const = 0;

	struct HoverResult
	{
		std::optional<ObjectID> new_hovered;
		std::optional<ObjectID> prev_hovered;
	};
	HoverResult process_hover(const Maths::Ray& ray);
	
private:
	TileObjectSpawner tile_spawner;
	TileSets tilesets;
	std::optional<ObjectID> prev_hovered;
	std::unordered_set<ObjectID> all_tiles;
};
