#include "tile_system.hpp"
#include "ecs.hpp"
#include "serialization/serializer.hpp"

#include <algorithm>


void TileSet::move_to_tile(const TileCoord& coord, Object& object)
{
	auto& tile = tiles[coord];
	tile.add_object(object.get_id());
	object_to_tile[object.get_id()] = &tile;
	object_to_coord[object.get_id()] = coord;

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
	object_to_coord.erase(object_id);
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
	const float gap = 0.01f; // gap between tiles to make the grid lines visible
	for (int row = 0; row < rows; ++row)
	{
		for (int col = 0; col < cols; ++col)
		{
			const TileCoord coord(col, row);
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
				tileset.coord_to_tile_object[coord] = tile.get_id();
				tileset.tile_object_to_coord[tile.get_id()] = coord;
			}
		}
	}
}

void TileSystem::remove_entity(const ObjectID& object_id)
{
	for (auto& [_, tileset] : tilesets)
	{
		tileset.remove_object(object_id);
		const auto tile_it = tileset.tile_object_to_coord.find(object_id);
		if (tile_it != tileset.tile_object_to_coord.end())
		{
			tileset.coord_to_tile_object.erase(tile_it->second);
			tileset.tile_object_to_coord.erase(tile_it);
		}
	}
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

std::optional<TileCoord> TileSystem::get_tile_coord(
	const ObjectID& object_id,
	const TileSetID& tileset_id) const
{
	const auto tileset_it = tilesets.find(tileset_id);
	if (tileset_it == tilesets.end())
		return std::nullopt;

	const auto& tileset = tileset_it->second;
	if (const auto tile = tileset.tile_object_to_coord.find(object_id);
		tile != tileset.tile_object_to_coord.end())
		return tile->second;
	if (const auto object = tileset.object_to_coord.find(object_id);
		object != tileset.object_to_coord.end())
		return object->second;
	return std::nullopt;
}

std::optional<ObjectID> TileSystem::get_tile_object(
	const TileCoord& coord,
	const TileSetID& tileset_id) const
{
	const auto tileset_it = tilesets.find(tileset_id);
	if (tileset_it == tilesets.end())
		return std::nullopt;

	const auto tile = tileset_it->second.coord_to_tile_object.find(coord);
	return tile == tileset_it->second.coord_to_tile_object.end()
		? std::nullopt
		: std::optional<ObjectID>(tile->second);
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
		prev_hovered.reset();
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

void TileSystem::serialize(Serializer& out) const
{
	auto system = out.map("tile_system");
	if (prev_hovered) system.write("previous_hovered", prev_hovered->get_underlying());
	else system.write_null("previous_hovered");
	std::vector<TileSetID> ids;
	for (const auto& [id, _] : tilesets) ids.push_back(id);
	std::ranges::sort(ids);
	auto sets_out = system.sequence("tilesets");
	for (const auto& id : ids) {
		const auto& tileset = tilesets.at(id);
		auto set_out = sets_out.append_map();
		set_out.write("tileset_id", id);
		set_out.write("cell_size", tileset.cell_size);
		std::vector<TileCoord> coords;
		for (const auto& [coord, _] : tileset.tiles) coords.push_back(coord);
		std::ranges::sort(coords, [](const TileCoord& lhs, const TileCoord& rhs) {
			return lhs.x < rhs.x || (lhs.x == rhs.x && lhs.y < rhs.y);
		});
		auto tiles_out = set_out.sequence("tiles");
		for (const auto coord : coords) {
			auto tile_out = tiles_out.append_map();
			auto coord_out = tile_out.map("coord");
			coord_out.write("x", coord.x);
			coord_out.write("y", coord.y);
			std::vector<ObjectID> objects(tileset.tiles.at(coord).objects.begin(), tileset.tiles.at(coord).objects.end());
			std::ranges::sort(objects);
			auto objects_out = tile_out.sequence("objects");
			for (const auto object : objects) objects_out.append(object.get_underlying());
		}
		std::vector<std::pair<TileCoord, ObjectID>> tile_objects(
			tileset.coord_to_tile_object.begin(), tileset.coord_to_tile_object.end());
		std::ranges::sort(tile_objects, [](const auto& lhs, const auto& rhs) {
			return lhs.first.x < rhs.first.x || (lhs.first.x == rhs.first.x && lhs.first.y < rhs.first.y);
		});
		auto tile_objects_out = set_out.sequence("tile_objects");
		for (const auto& [coord, object] : tile_objects) {
			auto entry = tile_objects_out.append_map();
			auto coord_out = entry.map("coord");
			coord_out.write("x", coord.x);
			coord_out.write("y", coord.y);
			entry.write("entity_id", object.get_underlying());
		}
	}
}

void TileSystem::deserialize(const Deserializer& in)
{
	const auto system = in.child("tile_system");
	TileSets restored_sets;
	std::unordered_set<ObjectID> restored_all_tiles;
	const auto sets = system.child("tilesets").elements();
	for (std::size_t set_index = 0; set_index < sets.size(); ++set_index) {
		const auto& set_in = sets[set_index];
		const auto id = set_in.read<std::string>("tileset_id");
		TileSet tileset;
		tileset.cell_size = set_in.read<float>("cell_size");
		const auto tile_entries = set_in.child("tiles").elements();
		for (std::size_t tile_index = 0; tile_index < tile_entries.size(); ++tile_index) {
			const auto& tile_in = tile_entries[tile_index];
			const auto coord_in = tile_in.child("coord");
			const TileCoord coord(coord_in.read<int>("x"), coord_in.read<int>("y"));
			Tile tile;
			for (const auto& object_in : tile_in.child("objects").elements()) {
				const ObjectID object(object_in.as<std::uint64_t>());
				if (!tile.objects.insert(object).second || tileset.object_to_coord.contains(object))
					throw SerializationError("Duplicate tile occupant at $.tile_system.tilesets["
						+ std::to_string(set_index) + "].tiles[" + std::to_string(tile_index) + "].objects");
				tileset.object_to_coord.emplace(object, coord);
			}
			if (!tileset.tiles.emplace(coord, std::move(tile)).second)
				throw SerializationError("Duplicate tile coordinate at $.tile_system.tilesets["
					+ std::to_string(set_index) + "].tiles[" + std::to_string(tile_index) + "].coord");
		}
		for (auto& [coord, tile] : tileset.tiles)
			for (const auto object : tile.objects)
				tileset.object_to_tile.emplace(object, &tile);

		const auto tile_objects = set_in.child("tile_objects").elements();
		for (std::size_t object_index = 0; object_index < tile_objects.size(); ++object_index) {
			const auto& object_in = tile_objects[object_index];
			const auto coord_in = object_in.child("coord");
			const TileCoord coord(coord_in.read<int>("x"), coord_in.read<int>("y"));
			const ObjectID object(object_in.read<std::uint64_t>("entity_id"));
			if (!tileset.coord_to_tile_object.emplace(coord, object).second
				|| !tileset.tile_object_to_coord.emplace(object, coord).second)
				throw SerializationError("Duplicate tile object at $.tile_system.tilesets["
					+ std::to_string(set_index) + "].tile_objects[" + std::to_string(object_index) + "]");
			restored_all_tiles.insert(object);
		}
		if (!restored_sets.emplace(id, std::move(tileset)).second)
			throw SerializationError("Duplicate tileset ID at $.tile_system.tilesets["
				+ std::to_string(set_index) + "].tileset_id");
	}
	const auto previous = system.child("previous_hovered");
	std::optional<ObjectID> restored_previous;
	if (previous.kind() != SerializationKind::Null) {
		restored_previous = ObjectID(previous.as<std::uint64_t>());
		if (!restored_all_tiles.contains(*restored_previous))
			throw SerializationError("Previous hovered tile is not a tile object at $.tile_system.previous_hovered");
	}
	tilesets = std::move(restored_sets);
	all_tiles = std::move(restored_all_tiles);
	prev_hovered = restored_previous;
}
