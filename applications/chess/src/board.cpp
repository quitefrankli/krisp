#include "board.hpp"

#include <resource_loader.hpp>
#include <utility.hpp>
#include <game_engine.hpp>
#include <shapes/shapes.hpp>

#include <cassert>
#include <iostream>


Tile::Tile(float x, float y, bool is_active_tile) : pos(x, y)
{
	const float size = 8; // TODO pass this from board
	const float tile_size = 5;
	const glm::vec3 pattern1 = glm::vec3(245, 245, 220)/256.0f;
	const glm::vec3 pattern2 = glm::vec3(186, 140, 99)/256.0f;

	Shapes::Square plane;
	if (is_active_tile)
	{
		plane.set_color(glm::vec3(0.1f, 1.0f, 0.0f));
	} else {
		plane.set_color((int(x+y))%2==0 ? pattern1 : pattern2);
	}
	plane.transform_vertices(glm::angleAxis(-Maths::PI/2.0f, Maths::right_vec));
	shapes.push_back(std::move(plane));
	set_position(glm::vec3((x-size/2.0f)*tile_size, 0, (y-size/2.0f)*tile_size));
	set_scale(glm::vec3(tile_size));
}

bool Tile::check_collision(const Maths::Ray& ray)
{
	const Maths::Plane plane(get_position(), get_rotation() * Maths::up_vec);
	glm::vec3 p = Maths::ray_plane_intersection(ray, plane);

	// check to make sure p is within bounds
	// use AABB style collision detection
	// first get p relative to current pos
	p = glm::abs(plane.offset - p);
	const float limit = get_scale().x / 2.0f; // assume square tile and unit mesh
	return p.x < limit && p.z < limit;
}


Board::Board(GameEngine& engine)
{
	ResourceLoader loader;
	auto chess_set = loader.load_objects(
		Utility::get().get_model_path().string() + "/chess_set.obj",
		std::vector<std::string_view>(33, Utility::get().get_textures_path().string() + "/chess_set.png"),
		ResourceLoader::Setting::ZERO_XZ);
	
	assert(chess_set.size() == 32+1);

	for (int y = 0; y < size; y++)
	{
		for (int x = 0; x < size; x++)
		{
			Tile* tile = &engine.spawn_object<Tile>(x, y);
			tiles.push_back(tile);
			if (y*size+x < 32)
			{
				// index 0  == chess board
				Piece& piece = engine.spawn_object<Piece>(std::move(chess_set[y*size+x+1]), nullptr);
				piece.move_to_tile(tile);
			}
		}
	}
	
}