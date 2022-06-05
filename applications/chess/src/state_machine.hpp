#pragma once

#include "board.hpp"
#include "chess_engine.hpp"

#include <memory>


class Object;
struct Initial;
struct PieceSelected;

struct State
{
	static std::unique_ptr<Initial> initial;
	static std::unique_ptr<PieceSelected> piece_selected;

	virtual State* process(Object&) = 0;

	static Board* board;
	static GameEngineT* engine;
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