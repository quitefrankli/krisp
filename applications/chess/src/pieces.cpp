#include "pieces.hpp"
#include "board.hpp"

#include <functional>


Piece::Piece(std::vector<Renderable> renderables, Type type, Side side) :
	Object(std::move(renderables)),
	type(type),
	side(side)
{}

bool Piece::check_collision(const Maths::Ray& ray, glm::vec3& intersection) const
{
	return (get_aabb()+get_position()).check_collision(ray, intersection);
}

std::vector<TileCoord> Piece::get_move_set(Type desired_type, const TileCoord& current_pos)
{
	std::vector<TileCoord> move_set;
	const int cur_x = current_pos.x;
	const int cur_y = current_pos.y;

	constexpr int board_size = 8;
	const auto validate_move = [](int x, int y)
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
			std::vector<TileCoord> knight_moves;
			knight_moves.emplace_back(cur_x + 1, cur_y + 2);
			knight_moves.emplace_back(cur_x + 1, cur_y - 2);
			knight_moves.emplace_back(cur_x - 1, cur_y + 2);
			knight_moves.emplace_back(cur_x - 1, cur_y - 2);
			knight_moves.emplace_back(cur_x + 2, cur_y + 1);
			knight_moves.emplace_back(cur_x + 2, cur_y - 1);
			knight_moves.emplace_back(cur_x - 2, cur_y + 1);
			knight_moves.emplace_back(cur_x - 2, cur_y - 1);
			for (auto& move : knight_moves)
			{
				if (validate_move(move.x, move.y))
					move_set.push_back(move);
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
						move_set.emplace_back(x, y);
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
					move_set.emplace_back(x, y);
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
			auto new_set = get_move_set(BISHOP, current_pos);
			move_set.insert(move_set.end(), new_set.begin(), new_set.end());
			new_set = get_move_set(ROOK, current_pos);
			move_set.insert(move_set.end(), new_set.begin(), new_set.end());
		}
		break;
	default:
		throw std::runtime_error("Piece::get_possible_tiles: invalid type");
	}

	return move_set;
}