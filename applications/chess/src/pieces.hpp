#pragma once

#include <objects/object.hpp>


class Tile;

class Piece : public Object
{
public:
	Piece(Object&& obj, Tile* tile);

	virtual bool move_to_tile(Tile* tile);

private:
	Tile* tile;
};