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

private:
	Tile* tile;
};