#include "board.hpp"

#include <resource_loader/resource_loader.hpp>
#include <utility.hpp>
#include <renderable/mesh_factory.hpp>
#include <entity_component_system/material_system.hpp>

#include <cassert>


Board::Board(PieceSpawner spawner, TileSystem& tile_system)
{
	auto loaded_model = ResourceLoader::load_model(Utility::get_model("chess_set_1k/chess_set_1k.gltf"));

	struct PieceInfo {
		Piece::Type type;
		int x, y;
		Piece::Side side;
	};

	static const std::vector<PieceInfo> piece_mapping = {
		{Piece::ROOK,   0, 0, Piece::Side::WHITE},
		{Piece::PAWN,   0, 1, Piece::Side::WHITE},
		{Piece::BISHOP, 2, 0, Piece::Side::WHITE},
		{Piece::QUEEN,  3, 0, Piece::Side::WHITE},
		{Piece::KING,   4, 0, Piece::Side::WHITE},
		{Piece::KNIGHT, 1, 0, Piece::Side::WHITE},
		{Piece::UNKNOWN, -1, -1, Piece::Side::WHITE},
		{Piece::KNIGHT, 6, 0, Piece::Side::WHITE},
		{Piece::ROOK,   7, 0, Piece::Side::WHITE},
		{Piece::BISHOP, 5, 0, Piece::Side::WHITE},
		{Piece::PAWN,   1, 1, Piece::Side::WHITE},
		{Piece::PAWN,   2, 1, Piece::Side::WHITE},
		{Piece::PAWN,   3, 1, Piece::Side::WHITE},
		{Piece::PAWN,   4, 1, Piece::Side::WHITE},
		{Piece::PAWN,   5, 1, Piece::Side::WHITE},
		{Piece::PAWN,   6, 1, Piece::Side::WHITE},
		{Piece::PAWN,   7, 1, Piece::Side::WHITE},
		{Piece::ROOK,   0, 7, Piece::Side::BLACK},
		{Piece::PAWN,   0, 6, Piece::Side::BLACK},
		{Piece::BISHOP, 2, 7, Piece::Side::BLACK},
		{Piece::QUEEN,  3, 7, Piece::Side::BLACK},
		{Piece::KING,   4, 7, Piece::Side::BLACK},
		{Piece::KNIGHT, 1, 7, Piece::Side::BLACK},
		{Piece::KNIGHT, 6, 7, Piece::Side::BLACK},
		{Piece::ROOK,   7, 7, Piece::Side::BLACK},
		{Piece::BISHOP, 5, 7, Piece::Side::BLACK},
		{Piece::PAWN,   1, 6, Piece::Side::BLACK},
		{Piece::PAWN,   2, 6, Piece::Side::BLACK},
		{Piece::PAWN,   3, 6, Piece::Side::BLACK},
		{Piece::PAWN,   4, 6, Piece::Side::BLACK},
		{Piece::PAWN,   5, 6, Piece::Side::BLACK},
		{Piece::PAWN,   6, 6, Piece::Side::BLACK},
		{Piece::PAWN,   7, 6, Piece::Side::BLACK},
	};

	assert(loaded_model.meshes.size() == piece_mapping.size());

	for (size_t i = 0; i < piece_mapping.size(); i++)
	{
		const auto& info = piece_mapping[i];
		if (info.type == Piece::UNKNOWN) // Board
		{
			auto& board = spawner(loaded_model.meshes[i].renderables, info.type, info.side);
			board.set_name("chess_board");
			board.set_scale(glm::vec3(85.0f));
			board.set_position(Maths::up_vec * -1.3f);
			continue;
		}

		auto& piece = spawner(loaded_model.meshes[i].renderables, info.type, info.side);
		piece.set_name(loaded_model.meshes[i].name);
		piece.set_scale(glm::vec3(100.0f));
		tile_system.move_to_tile(TileCoord(info.x, info.y), piece.get_id());

		const float TILE_SIZE = 5.0f;
		piece.set_position(glm::vec3((info.x-3.5f)*TILE_SIZE, 0, (info.y-3.5f)*TILE_SIZE));
	}
}
