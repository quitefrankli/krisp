#include "state_machine.hpp"


std::unique_ptr<Initial> State::initial = std::make_unique<Initial>();
std::unique_ptr<PieceSelected> State::piece_selected = std::make_unique<PieceSelected>();
Board* State::board = nullptr;

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
	}
	
	piece->get_tile()->highlight(true);
	const auto move_set = piece->get_move_set();
	for (auto move : move_set)
	{
		board->get_tile(move.first, move.second)->highlight(true);
	}

	return piece_selected.get();
}

State* PieceSelected::process(Object& object)
{
	Tile* tile = dynamic_cast<Tile*>(&object);
	if (!tile || tile == current_tile)
	{
		// nothing to do
		return this;
	}

	if (!tile->is_highlighted())
	{
		// invalid move
		return this;
	}

	for (auto* t : board->tiles)
	{
		t->highlight(false);
	}
	current_tile = nullptr;

	return initial.get();
}
