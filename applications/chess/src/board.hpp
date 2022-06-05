#pragma once

#include "chess_engine.hpp"
#include "pieces.hpp"

#include <objects/object.hpp>
#include <maths.hpp>


class ActiveTile;

class VisualTile : public Object
{
public:
	VisualTile(const glm::vec3& color, int size);

	virtual bool check_collision(const Maths::Ray& ray) override { return false; }
};

class Tile : public VisualTile
{
public:
	Tile(float x, float y);

	const std::pair<int, int> pos;
	Piece* piece = nullptr;
	virtual bool check_collision(const Maths::Ray& ray, glm::vec3& intersection) const override;
	void highlight(bool turn_on);
	VisualTile& get_highlighted_tile() { return highlighted; }
	bool is_highlighted() const;

private:
	static const float tile_size;
	static const glm::vec3 pattern1;
	static const glm::vec3 pattern2;
	// since there is no support for runtime colors currently, we can mimic it by toggling visibility of another tile
	VisualTile highlighted;
};

class Board : public Object
{
public:
	Board(GameEngineT& engine);

	Tile* get_tile(int x, int y)
	{
		if (x < 0 || x > size || y < 0 || y > size)
			throw std::runtime_error("Board::get_tile: err out of bounds!");
		return tiles[y*size+x];
	}

	std::vector<Tile*> tiles;

private:
	// std::vector<Piece&> pieces;
	const int size = 8;
};