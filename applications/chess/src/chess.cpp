#include "pieces.hpp"
#include "board.hpp"

#include <game_engine.hpp>
#include <window.hpp>
#include <config.hpp>
#include <utility.hpp>
#include <iapplication.hpp>
#include <camera.hpp>
#include <objects/cubemap.hpp>
#include <renderable/mesh_factory.hpp>
#include <renderable/material_factory.hpp>
#include <entity_component_system/light_source.hpp>

#include <fmt/core.h>
#include <fmt/color.h>
#include <GLFW/glfw3.h> // for key macros

#include <algorithm>
#include <optional>
#include <iostream>


class Application : public IApplication
{
public:
	virtual void on_tick(GameEngine& engine, float) override
	{
		// Hover highlighting runs each frame in the engine and can clear stenciling.
		// Re-apply selection highlights to keep legal moves visible while selected.
		for (const auto tile_id : highlighted_tiles)
		{
			if (auto* tile = engine.get_object(tile_id))
			{
				engine.highlight_object(*tile);
			}
		}
	}

	virtual void on_click(GameEngine& engine, Object& object) override
	{
		auto& ecs = engine.get_ecs();
		const auto clicked_coord = ecs.get_tile_coord(object.get_id());
		if (!clicked_coord)
		{
			clear_selection(engine);
			return;
		}

		if (selected_piece && is_legal_move(*clicked_coord))
		{
			ecs.move_to_tile(*clicked_coord, *selected_piece);
			if (auto* moved_piece = engine.get_object(*selected_piece))
			{
				moved_piece->set_position(tile_to_world_pos(*clicked_coord));
			}
			clear_selection(engine);
			return;
		}

		Piece* piece = get_piece_on_tile(ecs, *clicked_coord);
		if (!piece)
		{
			clear_selection(engine);
			return;
		}

		select_piece(engine, *piece, *clicked_coord);
	}

	virtual void on_begin(GameEngine& engine) override
	{
		engine.get_ecs().spawn_tileset(8, 8, 5.0f);
		board.emplace([&engine](std::vector<Renderable> renderables, Piece::Type type, Piece::Side side) -> Piece& {
			return engine.spawn_object<Piece>(std::move(renderables), type, side);
		}, engine.get_ecs());

		engine.get_camera().look_at(glm::vec3(0.0f), glm::vec3(0.0f, 20.0f, -50.0f));
	}

	virtual void on_key_press(GameEngine&, const KeyInput&) override {}

private:
	static constexpr int BOARD_SIZE = 8;
	static constexpr float TILE_SIZE = 5.0f;

	static bool in_bounds(const TileCoord& coord)
	{
		return coord.x >= 0 && coord.x < BOARD_SIZE && coord.y >= 0 && coord.y < BOARD_SIZE;
	}

	static glm::vec3 tile_to_world_pos(const TileCoord& coord)
	{
		return glm::vec3((coord.x - 3.5f) * TILE_SIZE, 0.0f, (coord.y - 3.5f) * TILE_SIZE);
	}

	static Piece* get_piece_on_tile(ECS& ecs, const TileCoord& coord)
	{
		auto* tile = ecs.get_tile(coord);
		if (!tile)
			return nullptr;

		for (const auto object_id : tile->get_objects())
		{
			Object& object = ecs.get_object(object_id);
			if (auto* piece = dynamic_cast<Piece*>(&object))
				return piece;
		}

		return nullptr;
	}

	std::vector<TileCoord> get_legal_moves(ECS& ecs, Piece& piece, const TileCoord& from) const
	{
		auto moves = piece.get_move_set(from);
		std::vector<TileCoord> legal_moves;
		legal_moves.reserve(moves.size());

		for (const auto& move : moves)
		{
			if (!in_bounds(move))
				continue;

			const Piece* occupying_piece = get_piece_on_tile(ecs, move);
			if (occupying_piece && occupying_piece->side == piece.side)
				continue;

			legal_moves.push_back(move);
		}

		return legal_moves;
	}

	void clear_selection(GameEngine& engine)
	{
		for (const auto tile_id : highlighted_tiles)
		{
			if (auto* tile = engine.get_object(tile_id))
			{
				engine.unhighlight_object(*tile);
			}
		}

		selected_piece.reset();
		legal_moves.clear();
		highlighted_tiles.clear();
	}

	bool is_legal_move(const TileCoord& coord) const
	{
		return std::find(legal_moves.begin(), legal_moves.end(), coord) != legal_moves.end();
	}

	void select_piece(GameEngine& engine, Piece& piece, const TileCoord& from)
	{
		clear_selection(engine);
		selected_piece = piece.get_id();
		legal_moves = get_legal_moves(engine.get_ecs(), piece, from);

		for (const auto& move : legal_moves)
		{
			const auto tile_id = engine.get_ecs().get_tile_object(move);
			if (!tile_id)
				continue;

			auto* tile = engine.get_object(*tile_id);
			if (!tile)
				continue;

			engine.highlight_object(*tile);
			highlighted_tiles.push_back(*tile_id);
		}
	}

	std::optional<Board> board;
	std::optional<ObjectID> selected_piece;
	std::vector<TileCoord> legal_moves;
	std::vector<ObjectID> highlighted_tiles;
};

int main(int argc, char* argv[])
{
	Config::init(PROJECT_NAME);

	auto engine = GameEngine::create<Application>();
	auto& ecs = engine.get_ecs();

	// Add skybox
	engine.spawn_object<CubeMap>();

	// Add light source
	auto& light_source = engine.spawn_object<Object>(Renderable{
		.mesh_id = MeshFactory::sphere_id(),
		.material_ids = { MaterialFactory::fetch_preset(EMaterialPreset::LIGHT_SOURCE) },
		.pipeline_render_type = ERenderType::COLOR,
		.casts_shadow = false
	});
	light_source.set_position(glm::vec3(0.0f, 10.0f, 0.0f));
	light_source.set_scale(glm::vec3(2.0f));

	ecs.add_collider(light_source.get_id(), std::make_unique<SphereCollider>());
	ecs.add_clickable_entity(light_source.get_id());

	LightComponent light;
	light.intensity = 1.0f;
	light.color = glm::vec3(1.0f, 0.95f, 0.9f);
	ecs.add_light_source(light_source.get_id(), light);

	engine.run();
}
