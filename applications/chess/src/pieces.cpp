#include "pieces.hpp"
#include "board.hpp"

#include <glm/gtx/string_cast.hpp>

#include <iostream>


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

bool Piece::check_collision(const Maths::Ray& ray, glm::vec3& intersection) const
{
	return (get_aabb()+get_position()).check_collision(ray, intersection);
}