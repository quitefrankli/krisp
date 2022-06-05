#pragma once

#include "board.hpp"

#include <memory>


class Object;
template<typename GameEngineT>
class Initial;
template<typename GameEngineT>
class PieceSelected;

template<typename GameEngineT>
struct State
{
	static std::unique_ptr<Initial> initial;
	static std::unique_ptr<PieceSelected> piece_selected;

	virtual State* process(Object&) = 0;

	static Board* board;
	static GameEngineT* engine;
};

// when nothing is selected
template<typename GameEngineT>
struct Initial : public State<GameEngineT>
{
	virtual State<GameEngineT>* process(Object&) override;

};

// when a piece gets selected
template<typename GameEngineT>
struct PieceSelected : public State<GameEngineT>
{
	virtual State<GameEngineT>* process(Object&) override;

	Tile* current_tile = nullptr;
};