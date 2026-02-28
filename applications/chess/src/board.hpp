#pragma once

#include "pieces.hpp"

#include <entity_component_system/tile_system.hpp>
#include <objects/object.hpp>
#include <maths.hpp>

#include <functional>


class Board
{
public:
	using PieceSpawner = std::function<Piece&(std::vector<Renderable>, Piece::Type, Piece::Side)>;

	Board(PieceSpawner spawner, TileSystem& tile_system);

	std::unordered_map<Piece::Type, Piece*> pieces;
};
