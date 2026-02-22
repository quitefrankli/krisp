#include "board.hpp"

#include <resource_loader/resource_loader.hpp>
#include <utility.hpp>
#include <game_engine.hpp>
#include <renderable/mesh_factory.hpp>
#include <entity_component_system/material_system.hpp>

#include <cassert>
#include <iostream>


VisualTile::VisualTile(const glm::vec3& color, int size)
{
	Renderable& renderable = renderables.emplace_back();
	renderable.mesh_id = MeshFactory::quad_id();
	ColorMaterial material;
	material.data.diffuse = color;
	renderable.material_ids = { MaterialSystem::add(std::make_unique<ColorMaterial>(std::move(material))) };
	set_scale(glm::vec3(size));
	set_rotation(glm::angleAxis(-Maths::PI/2.0f, Maths::right_vec));
}

const float Tile::tile_size = 5;
const glm::vec3 Tile::pattern1 = glm::vec3(245, 245, 220)/256.0f;
const glm::vec3 Tile::pattern2 = glm::vec3(186, 140, 99)/256.0f;

Tile::Tile(float x, float y) : 
	VisualTile(int(x+y)%2 == 0 ? pattern1 : pattern2, tile_size),
	highlighted(glm::vec3(0.1f, 1.0f, 0.0f), tile_size),
	pos(x, y)
{
	const float size = 8; // TODO pass this from board
	set_position(glm::vec3((x-size/2.0f)*tile_size, 0, (y-size/2.0f)*tile_size));
	highlighted.set_position(get_position()+Maths::up_vec*0.01f); // offset it a little
	highlighted.set_visibility(false);
}

bool Tile::check_collision(const Maths::Ray& ray, glm::vec3& intersection) const
{
	const Maths::Plane plane(get_position(), get_rotation() * Maths::up_vec);
	intersection = Maths::ray_plane_intersection(ray, plane);

	// check to make sure p is within bounds
	// use AABB style collision detection
	// first get p relative to current pos
	const glm::vec3 p = glm::abs(plane.offset - intersection);
	const float limit = get_scale().x / 2.0f; // assume square tile and unit mesh
	return p.x < limit && p.z < limit;
}

void Tile::highlight(bool turn_on)
{
	highlighted.set_visibility(turn_on);
}

bool Tile::is_highlighted() const
{
	return highlighted.get_visibility();
}

Board::Board(GameEngine& engine)
{
	// Create board tiles
	for (int y = 0; y < size; y++)
	{
		for (int x = 0; x < size; x++)
		{
			Tile* tile = &engine.spawn_object<Tile>(x, y);
			engine.draw_object(tile->get_highlighted_tile());
			tiles.push_back(tile);
		}
	}

	// Load chess pieces from glTF model
	auto loaded_model = ResourceLoader::load_model(Utility::get_model("chess_set_2k.gltf"));

	// Mapping of renderable index to {piece_type, {x, y}, side}
	// Based on the glTF node order from chess_set_2k.gltf
	struct PieceInfo {
		int type;
		int x, y;
		Piece::Side side;
	};

	const std::vector<PieceInfo> piece_mapping = {
		// White pieces (indices 0-16, skipping 6 which is the board)
		{Piece::ROOK,   0, 0, Piece::Side::WHITE},  // 0: rook_white_01
		{Piece::PAWN,   0, 1, Piece::Side::WHITE},  // 1: pawn_white_01
		{Piece::BISHOP, 2, 0, Piece::Side::WHITE},  // 2: bishop_white_01
		{Piece::QUEEN,  3, 0, Piece::Side::WHITE},  // 3: queen_white
		{Piece::KING,   4, 0, Piece::Side::WHITE},  // 4: king_white
		{Piece::KNIGHT, 1, 0, Piece::Side::WHITE},  // 5: knight_white_01
		{Piece::UNKNOWN, -1, -1, Piece::Side::WHITE}, // 6: board (skip)
		{Piece::KNIGHT, 6, 0, Piece::Side::WHITE},  // 7: knight_white_02
		{Piece::ROOK,   7, 0, Piece::Side::WHITE},  // 8: rook_white_02
		{Piece::BISHOP, 5, 0, Piece::Side::WHITE},  // 9: bishop_white_02
		{Piece::PAWN,   1, 1, Piece::Side::WHITE},  // 10: pawn_white_02
		{Piece::PAWN,   2, 1, Piece::Side::WHITE},  // 11: pawn_white_03
		{Piece::PAWN,   3, 1, Piece::Side::WHITE},  // 12: pawn_white_04
		{Piece::PAWN,   4, 1, Piece::Side::WHITE},  // 13: pawn_white_05
		{Piece::PAWN,   5, 1, Piece::Side::WHITE},  // 14: pawn_white_06
		{Piece::PAWN,   6, 1, Piece::Side::WHITE},  // 15: pawn_white_07
		{Piece::PAWN,   7, 1, Piece::Side::WHITE},  // 16: pawn_white_08
		// Black pieces (indices 17-32)
		{Piece::ROOK,   0, 7, Piece::Side::BLACK},  // 17: rook_black_01
		{Piece::PAWN,   0, 6, Piece::Side::BLACK},  // 18: pawn_black_01
		{Piece::BISHOP, 2, 7, Piece::Side::BLACK},  // 19: bishop_black_01
		{Piece::QUEEN,  3, 7, Piece::Side::BLACK},  // 20: queen_black
		{Piece::KING,   4, 7, Piece::Side::BLACK},  // 21: king_black
		{Piece::KNIGHT, 1, 7, Piece::Side::BLACK},  // 22: knight_black_01
		{Piece::KNIGHT, 6, 7, Piece::Side::BLACK},  // 23: knight_black_02
		{Piece::ROOK,   7, 7, Piece::Side::BLACK},  // 24: rook_black_02
		{Piece::BISHOP, 5, 7, Piece::Side::BLACK},  // 25: bishop_black_02
		{Piece::PAWN,   1, 6, Piece::Side::BLACK},  // 26: pawn_black_02
		{Piece::PAWN,   2, 6, Piece::Side::BLACK},  // 27: pawn_black_03
		{Piece::PAWN,   3, 6, Piece::Side::BLACK},  // 28: pawn_black_04
		{Piece::PAWN,   4, 6, Piece::Side::BLACK},  // 29: pawn_black_05
		{Piece::PAWN,   5, 6, Piece::Side::BLACK},  // 30: pawn_black_06
		{Piece::PAWN,   6, 6, Piece::Side::BLACK},  // 31: pawn_black_07
		{Piece::PAWN,   7, 6, Piece::Side::BLACK},  // 32: pawn_black_08
	};

	// assert(loaded_model.renderables.size() == piece_mapping.size() && "Chess model should have 33 renderables");

	// Spawn each piece
	for (size_t i = 0; i < piece_mapping.size(); i++)
	{
		const auto& info = piece_mapping[i];

		// Skip the board renderable (index 6)
		if (info.type == Piece::UNKNOWN)
			continue;

		// Create object from renderable
		Object temp_obj(loaded_model.renderables[i]);

		// Get the tile for this piece
		Tile* tile = get_tile(info.x, info.y);

		// Spawn the piece
		Piece& piece = engine.spawn_object<Piece>(std::move(temp_obj), tile);
		piece.type = info.type;
		piece.side = info.side;
		piece.move_to_tile(tile);
	}
}
