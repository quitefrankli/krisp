#pragma once

#include "pieces.hpp"

#include <objects/object.hpp>
#include <maths.hpp>

#include <functional>

class ECS;

class Board
{
public:
	using PieceSpawner = std::function<Piece&(std::vector<Renderable>, Piece::Type, Piece::Side)>;

	Board(PieceSpawner spawner, ECS& ecs);

	std::unordered_map<Piece::Type, Piece*> pieces;
};
