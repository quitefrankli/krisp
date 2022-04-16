#pragma once

#include <objects/object.hpp>


class Tile;

class Piece : public Object
{
public:
	Piece(Object&& obj, Tile* tile);

	enum {
		ROOK,
		KNIGHT,
		BISHOP,
		KING,
		QUEEN,
		PAWN,
		UNKNOWN,
	};

	int type;

	virtual bool move_to_tile(Tile* tile);

	virtual bool check_collision(const Maths::Ray& ray, glm::vec3& intersection) const override;

private:
	Tile* tile;
};