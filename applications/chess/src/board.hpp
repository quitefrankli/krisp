#pragma once

#include "pieces.hpp"

#include <entity_component_system/tile_system.hpp>
#include <objects/object.hpp>
#include <maths.hpp>

#include <functional>


class ActiveBoardSquare;

class VisualTile : public Object
{
public:
	VisualTile(const glm::vec3& color, int size);

	virtual bool check_collision(const Maths::Ray& ray) override { return false; }
};

class BoardSquare : public VisualTile
{
public:
	BoardSquare(float x, float y);

	const TileCoord pos;
	virtual bool check_collision(const Maths::Ray& ray, glm::vec3& intersection) const override;
	void highlight(bool turn_on);
	VisualTile& get_highlighted_tile() { return highlighted; }
	bool is_highlighted() const;

	static const float tile_size;
private:
	static const glm::vec3 pattern1;
	static const glm::vec3 pattern2;
	VisualTile highlighted;
};

class Board
{
public:
	using PieceSpawner = std::function<Piece&(std::vector<Renderable>, Piece::Type, Piece::Side)>;

	Board(PieceSpawner spawner);

	BoardSquare* get_square(int x, int y)
	{
		if (x < 0 || x >= size || y < 0 || y >= size)
			throw std::runtime_error("Board::get_square: err out of bounds!");
		return squares[y*size+x];
	}

	BoardSquare* get_square(const TileCoord& coord) { return get_square(coord.x, coord.y); }

	std::vector<BoardSquare*> squares;

	static constexpr int size = 8;
};