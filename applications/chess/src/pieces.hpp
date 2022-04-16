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

	enum class Side {
		WHITE,
		BLACK
	};

	int type;

	virtual bool move_to_tile(Tile* tile);

	virtual bool check_collision(const Maths::Ray& ray, glm::vec3& intersection) const override;

	Tile* get_tile() { return tile; }

	std::vector<std::pair<int, int>> get_move_set() { return get_move_set(type); }

	Side side = Side::WHITE;

private:
	std::vector<std::pair<int, int>> get_move_set(int desired_type);
	Tile* tile;
};