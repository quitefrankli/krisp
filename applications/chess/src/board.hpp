#pragma once

#include "pieces.hpp"

#include <objects/object.hpp>
#include <maths.hpp>


class GameEngine;

class Tile : public Object
{
public:
	Tile(float x, float y, bool is_active_tile=false);

	Piece* piece = nullptr;

	std::pair<int, int> get_pos() const { return pos; }

	virtual bool check_collision(const Maths::Ray& ray) override;

private:
	const std::pair<int, int> pos;
};

class Board : public Object
{
public:
	Board(GameEngine& engine);

	Tile* get_tile(int x, int y)
	{
		if (x < 0 || x > size || y < 0 || y > size)
			throw std::runtime_error("Board::get_tile: err out of bounds!");
		return tiles[y*size+x];
	}

private:
	std::vector<Tile*> tiles;
	// std::vector<Piece&> pieces;
	const int size = 8;
};