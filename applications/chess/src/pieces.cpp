#include "pieces.hpp"
#include "board.hpp"

#include <glm/gtx/string_cast.hpp>

#include <functional>
#include <iostream>


Piece::Piece(Object&& obj, Tile* tile) :
	Object::Object(std::move(obj)),
	tile(tile)
{}

bool Piece::move_to_tile(Tile* tile)
{
	if (this->tile)
		this->tile->piece = nullptr;
	this->tile = tile;
	tile->piece = this;

	set_position(tile->get_position());

	return true;
}

bool Piece::check_collision(const Maths::Ray& ray, glm::vec3& intersection) const
{
	return (get_aabb()+get_position()).check_collision(ray, intersection);
}

std::vector<std::pair<int, int>> Piece::get_move_set(int desired_type)
{
	std::vector<std::pair<int, int>> move_set;
	int cur_x = tile->pos.first;
	int cur_y = tile->pos.second;

	// TODO: have some global constant for this
	const int board_size = 8;
	const auto validate_move = [board_size](int x, int y)
	{
		return x >= 0 && x < board_size && y >= 0 && y < board_size;
	};

	switch (desired_type)
	{
	case PAWN:
		{
			const int inc = side == Side::WHITE ? 1 : -1;
			move_set.emplace_back(cur_x, cur_y+inc);
		}
		break;
	case ROOK:
		for (int y = 0; y < board_size; y++)
		{
			if (y == cur_y)
				continue;
			move_set.emplace_back(cur_x, y);
		}
		for (int x = 0; x < board_size; x++)
		{
			if (x == cur_x)
				continue;
			move_set.emplace_back(x, cur_y);
		}
		break;
	case KNIGHT:
		{
			std::vector<std::pair<int, int>> knight_moves;
			knight_moves.emplace_back(cur_x + 1, cur_y + 2);
			knight_moves.emplace_back(cur_x + 1, cur_y - 2);
			knight_moves.emplace_back(cur_x - 1, cur_y + 2);
			knight_moves.emplace_back(cur_x - 1, cur_y - 2);
			knight_moves.emplace_back(cur_x + 2, cur_y + 1);
			knight_moves.emplace_back(cur_x + 2, cur_y - 1);
			knight_moves.emplace_back(cur_x - 2, cur_y + 1);
			knight_moves.emplace_back(cur_x - 2, cur_y - 1);
			for (auto move : knight_moves)
			{
				if (validate_move(move.first, move.second))
				{
					move_set.push_back(std::move(move));
				}
			}
		}
		break;
	case BISHOP:
		{
			const auto loop = [&](std::function<void(int&, int&)>&& fn)
			{
				int x = cur_x;
				int y = cur_y;
				while (validate_move(x, y))
				{
					if (x != cur_x || y != cur_y)
					{
						move_set.emplace_back(x, y);
					}
					fn(x, y);
				}
			};
			loop([](int& x, int& y){ x++; y++; });
			loop([](int& x, int& y){ x++; y--; });
			loop([](int& x, int& y){ x--; y++; });
			loop([](int& x, int& y){ x--; y--; });
		}
		break;
	case KING:
		{
			const auto add_move = [&](int x, int y)
			{
				if (validate_move(x, y))
				{
					move_set.emplace_back(x, y);
				}
			};
			add_move(cur_x, cur_y+1);
			add_move(cur_x, cur_y-1);
			add_move(cur_x+1, cur_y);
			add_move(cur_x-1, cur_y);
			
			add_move(cur_x+1, cur_y+1);
			add_move(cur_x+1, cur_y-1);
			add_move(cur_x-1, cur_y+1);
			add_move(cur_x-1, cur_y-1);
		}
		break;
	case QUEEN:
		{
			auto new_set = get_move_set(BISHOP);
			move_set.insert(move_set.end(), new_set.begin(), new_set.end());
			new_set = get_move_set(ROOK);
			move_set.insert(move_set.end(), new_set.begin(), new_set.end());
		}
		break;
	default:
		throw std::runtime_error("Piece::get_possible_tiles: invalid type");
	}

	return move_set;
}