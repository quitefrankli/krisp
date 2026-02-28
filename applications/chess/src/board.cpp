#include "board.hpp"

#include <resource_loader/resource_loader.hpp>
#include <utility.hpp>
#include <renderable/mesh_factory.hpp>
#include <entity_component_system/material_system.hpp>

#include <cassert>


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

const float BoardSquare::tile_size = 5;
const glm::vec3 BoardSquare::pattern1 = glm::vec3(245, 245, 220)/256.0f;
const glm::vec3 BoardSquare::pattern2 = glm::vec3(186, 140, 99)/256.0f;

BoardSquare::BoardSquare(float x, float y) :
	VisualTile(int(x+y)%2 == 0 ? pattern1 : pattern2, tile_size),
	highlighted(glm::vec3(0.1f, 1.0f, 0.0f), tile_size),
	pos(x, y)
{
	set_position(glm::vec3((x-Board::size/2.0f)*tile_size, 0, (y-Board::size/2.0f)*tile_size));
	highlighted.set_position(get_position()+Maths::up_vec*0.01f);
	highlighted.set_visibility(false);
}

bool BoardSquare::check_collision(const Maths::Ray& ray, glm::vec3& intersection) const
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

void BoardSquare::highlight(bool turn_on)
{
	highlighted.set_visibility(turn_on);
}

bool BoardSquare::is_highlighted() const
{
	return highlighted.get_visibility();
}

Board::Board(PieceSpawner spawner)
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

		piece.set_position(glm::vec3((info.x-3.5f)*BoardSquare::tile_size, 0, (info.y-3.5f)*BoardSquare::tile_size));
	}
}
