#include "pieces.hpp"
#include "board.hpp"


Piece::Piece(Object&& obj, Tile* tile) :
	Object::Object(std::move(obj)),
	tile(tile)
{}

bool Piece::move_to_tile(Tile* tile)
{
	if (this->tile)
		this->tile->piece = nullptr;
	this->tile = tile;
	tile->piece = this;

	set_position(tile->get_position());

	return true;
}
