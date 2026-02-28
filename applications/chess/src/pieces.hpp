#pragma once

#include <entity_component_system/tile_system.hpp>
#include <objects/object.hpp>


class BoardSquare;

class Piece : public Object
{
public:
	enum Type {
		ROOK,
		KNIGHT,
		BISHOP,
		KING,
		QUEEN,
		PAWN,
		UNKNOWN
	};

	enum class Side {
		WHITE,
		BLACK
	};

	Piece(std::vector<Renderable> renderables, Type type, Side side);

	Type type;
	Side side;

	virtual bool check_collision(const Maths::Ray& ray, glm::vec3& intersection) const override;

	std::vector<TileCoord> get_move_set(const TileCoord& current_pos) { return get_move_set(type, current_pos); }

private:
	std::vector<TileCoord> get_move_set(Type desired_type, const TileCoord& current_pos);
};