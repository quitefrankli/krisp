#include "state_machine.hpp"

#include <game_engine.hpp>

#include <iostream>


std::unique_ptr<Initial> State::initial = std::make_unique<Initial>();
std::unique_ptr<PieceSelected> State::piece_selected = std::make_unique<PieceSelected>();
Board* State::board = nullptr;
GameEngineT* State::engine = nullptr;

State* Initial::process(Object& object)
{
	Piece* piece = dynamic_cast<Piece*>(&object);
	if (!piece)
	{
		Tile* tile = dynamic_cast<Tile*>(&object);
		if (!tile)
		{
			// nothing clicked on
			return this;
		}

		piece = tile->piece;
		// tile has no piece
		if (!piece)
		{
			return this;
		}
	}
	
	piece->get_tile()->highlight(true);
	const auto move_set = piece->get_move_set();
	for (auto move : move_set)
	{
		board->get_tile(move.first, move.second)->highlight(true);
	}
	piece_selected->current_tile = piece->get_tile();

	return piece_selected.get();
}

State* PieceSelected::process(Object& object)
{
	Piece* piece = dynamic_cast<Piece*>(&object);
	Tile* tile = nullptr;
	if (!piece)
	{
		tile = dynamic_cast<Tile*>(&object);
		if (!tile)
		{
			// nothing clicked on
			return this;
		}

		piece = tile->piece;
	} else {
		tile = piece->get_tile();
	}

	if (tile->is_highlighted() && tile != current_tile)
	{
		if (tile->piece)
		{
			engine->delete_object(tile->piece->get_id());
			tile->piece = nullptr;
		}
		current_tile->piece->move_to_tile(tile);
	}

	for (auto* t : board->tiles)
	{
		t->highlight(false);
	}
	current_tile = nullptr;

	return initial.get();
}
