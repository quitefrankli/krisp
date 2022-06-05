#include "board.hpp"

#include <resource_loader.hpp>
#include <utility.hpp>
#include <game_engine.hpp>
#include <shapes/shapes.hpp>

#include <cassert>
#include <iostream>


VisualTile::VisualTile(const glm::vec3& color, int size)
{
	Shapes::Square plane;
	plane.set_color(color);
	plane.transform_vertices(glm::angleAxis(-Maths::PI/2.0f, Maths::right_vec));
	shapes.push_back(std::move(plane));
	set_scale(glm::vec3(size));
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

Board::Board(GameEngineT& engine)
{
	ResourceLoader loader;
	auto chess_set = loader.load_objects(
		Utility::get().get_model_path().string() + "/chess_set.obj",
		std::vector<std::string_view>(33, Utility::get().get_textures_path().string() + "/chess_set.png"),
		ResourceLoader::Setting::ZERO_XZ);
	
	const std::vector<std::pair<int, std::pair<int, int>>> mapping {
		{Piece::KING, {0,3}}, {Piece::PAWN, {1,0}}, {Piece::PAWN, {1,1}}, {Piece::PAWN, {1,2}}, 
		{Piece::PAWN, {1,3}}, {Piece::PAWN, {1,4}}, {Piece::PAWN, {1,5}}, {Piece::PAWN, {1,6}},
		{Piece::PAWN, {1,7}}, {Piece::QUEEN, {0,4}}, {Piece::ROOK, {0,0}}, {Piece::ROOK, {0,7}}, 
		{Piece::BISHOP, {0,2}}, {Piece::BISHOP, {0,5}}, {Piece::KNIGHT, {0,1}}, {Piece::KNIGHT, {0,6}},
		{Piece::KING, {7,3}}, {Piece::QUEEN, {7,4}}, {Piece::KNIGHT, {7,1}}, {Piece::KNIGHT, {7,6}},
		{Piece::BISHOP, {7,2}}, {Piece::BISHOP, {7,5}}, {Piece::ROOK, {7,0}}, {Piece::ROOK, {7,7}},
		{Piece::PAWN, {6,0}}, {Piece::PAWN, {6,1}}, {Piece::PAWN, {6,2}}, {Piece::PAWN, {6,3}}, 
		{Piece::PAWN, {6,4}}, {Piece::PAWN, {6,5}}, {Piece::PAWN, {6,6}}, {Piece::PAWN, {6,7}},
	};

	assert(chess_set.size() - 1 == mapping.size() && mapping.size() == 32);
	for (int y = 0; y < size; y++)
	{
		for (int x = 0; x < size; x++)
		{
			Tile* tile = &engine.spawn_object<Tile>(x, y);
			engine.draw_object(tile->get_highlighted_tile());
			tiles.push_back(tile);
		}
	}

	for (int i = 0; i < mapping.size(); i++)
	{
		const auto val = mapping[i];
		// index 0  == chess board
		Piece& piece = engine.spawn_object<Piece>(std::move(chess_set[i+1]), nullptr);
		piece.type = val.first;
		piece.move_to_tile(get_tile(val.second.second, val.second.first));
		piece.side = i < mapping.size()/2 ? Piece::Side::WHITE : Piece::Side::BLACK;
	}
}