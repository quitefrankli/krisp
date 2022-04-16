#pragma once

#include "board.hpp"

#include <memory>


class Object;
class Initial;
class PieceSelected;
class GameEngine;

struct State
{
	static std::unique_ptr<Initial> initial;
	static std::unique_ptr<PieceSelected> piece_selected;

	virtual State* process(Object&) = 0;

	static Board* board;
	static GameEngine* engine;
};

// when nothing is selected
struct Initial : public State 
{
	virtual State* process(Object&) override;

};

// when a piece gets selected
struct PieceSelected : public State
{
	virtual State* process(Object&) override;

	Tile* current_tile = nullptr;
};