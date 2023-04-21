#include <game_engine.hpp>
#include <graphics_engine/graphics_engine.hpp>
#include <iapplication.hpp>
#include <resource_loader.hpp>
#include <utility.hpp>
#include <camera.hpp>
#include <objects/light_source.hpp>
#include <shapes/shapes.hpp>
#include <config.hpp>
#include <shapes/shape_factory.hpp>

#include <fmt/core.h>
#include <fmt/color.h>
#include <glm/gtx/string_cast.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/hash.hpp"

#include <iostream>


using GameEngineT = GameEngine<GraphicsEngine>;

constexpr int height = 20;
constexpr int width = 10;

// includes floor and walls
class Environment : public Object
{
public:
	Environment()
	{
		glm::mat4 transform = glm::mat4(1.0f);
		Material material;
		material.material_data.specular = glm::vec3(0.0f, 0.0f, 0.0f);

		Shape floor = Shapes::Cube();
		transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -10.75f, 0.0f));
		transform = glm::scale(transform, glm::vec3(width*2, 1.5f, 3.0f));
		material.material_data.diffuse = glm::vec3(0.5f, 0.3f, 0.2f);
		floor.set_material(material);
		floor.transform_vertices(transform);

		Shape left_wall = Shapes::Cube();
		transform = glm::translate(glm::mat4(1.0f), glm::vec3(-width/2, 0.0f, 0.0f));
		transform = glm::scale(transform, glm::vec3(1.0f, height, 3.0f));
		material.material_data.diffuse = glm::vec3(0.2f, 0.3f, 0.2f);
		left_wall.set_material(material);
		left_wall.transform_vertices(transform);

		Shape right_wall = Shapes::Cube();
		transform = glm::translate(glm::mat4(1.0f), glm::vec3(width/2, 0.0f, 0.0f));
		transform = glm::scale(transform, glm::vec3(1.0f, height, 3.0f));
		right_wall.set_material(material);
		right_wall.transform_vertices(transform);

		Shape back_wall = Shapes::Cube();
		transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 2.0f));
		transform = glm::scale(transform, glm::vec3(width, height, 1.0f));
		material.material_data.diffuse = glm::vec3(0.2f, 0.3f, 0.2f);
		back_wall.set_material(material);
		back_wall.transform_vertices(transform);

		shapes.push_back(std::move(floor));
		shapes.push_back(std::move(left_wall));
		shapes.push_back(std::move(right_wall));
		shapes.push_back(std::move(back_wall));
	}
};

enum class TetrisPieceType
{
	I,
	J,
	L,
	O,
	S,
	T,
	Z
};

std::vector<Shape> generate_shapes(const std::vector<int>& locations)
{
	// given a 4x4 grid, the locations are:
	// 0  1  2  3
	// 4  5  6  7
	// 8  9  10 11
	// 12 13 14 15
	// for every location a cube will be inserted there
	// the origin of the grid is at the bottom left

	std::vector<Shape> shapes;
	for (auto location : locations)
	{
		auto x = location % 4;
		auto y = 3 - location / 4;

		auto shape = Shapes::Cube();
		shape.translate_vertices(glm::vec3(x, y, 0.0f));
		shapes.push_back(std::move(shape));
	}

	return shapes;
}

const std::vector<glm::vec3> standard_colors =
{
	glm::vec3(0.0f, 1.0f, 1.0f),
	glm::vec3(0.0f, 0.0f, 1.0f),
	glm::vec3(1.0f, 0.5f, 0.0f),
	glm::vec3(1.0f, 1.0f, 0.0f),
	glm::vec3(0.0f, 1.0f, 0.0f),
	glm::vec3(1.0f, 0.0f, 1.0f),
	glm::vec3(1.0f, 0.0f, 0.0f)
};

class TetrisPiece : public Object
{
public:
	TetrisPiece(TetrisPieceType type, const glm::vec3& color) :
		type(type)
	{
		const auto per_shape_fn = [&](const glm::vec3& translation)
		{
			Material material;
			material.material_data.specular = Maths::zero_vec;
			material.material_data.diffuse = color;
			std::for_each(shapes.begin(), shapes.end(), [&material, &translation](Shape& shape) { 
				shape.translate_vertices(translation); 
				shape.set_material(material);
			});
		};

		switch (type)
		{
		case TetrisPieceType::I:
			shapes = generate_shapes({ 12, 13, 14, 15 });
			// move so that origin matches the rotation point
			per_shape_fn(glm::vec3(-1.5f, 0.5f, 0.0f));
			cell_locations = { { -1.5f, 0.5f }, { -0.5f, 0.5f }, { 0.5f, 0.5f }, { 1.5f, 0.5f } };
			type_specific_offset = glm::vec3(0.5f, 0.5f, 0.0f);
			break;
		case TetrisPieceType::J:
			shapes = generate_shapes({ 8, 12, 13, 14 });
			per_shape_fn(glm::vec3(-1.0f, 0.0f, 0.0f));
			cell_locations = { { -1.0f, 1.0f }, { -1.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f } };
			break;
		case TetrisPieceType::L:
			shapes = generate_shapes({ 10, 12, 13, 14 });
			per_shape_fn(glm::vec3(-1.0f, 0.0f, 0.0f)); 
			cell_locations = { { -1.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f } };
			break;
		case TetrisPieceType::O:
			shapes = generate_shapes({ 8, 9, 12, 13 });
			per_shape_fn(glm::vec3(-0.5f, -0.5f, 0.0f)); 
			cell_locations = { { -0.5f, 0.5f }, { 0.5f, 0.5f }, { -0.5f, -0.5f }, { 0.5f, -0.5f } };
			type_specific_offset = glm::vec3(0.5f, 0.5f, 0.0f);
			break;
		case TetrisPieceType::S:
			shapes = generate_shapes({ 9, 10, 12, 13 });
			per_shape_fn(glm::vec3(-1.0f, 0.0f, 0.0f)); 
			cell_locations = { { -1.0f, 0.0f }, { 0.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f } };
			break;
		case TetrisPieceType::T:
			shapes = generate_shapes({ 9, 12, 13, 14 });
			per_shape_fn(glm::vec3(-1.0f, 0.0f, 0.0f)); 
			cell_locations = { { -1.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f } };
			break;
		case TetrisPieceType::Z:
			shapes = generate_shapes({ 8, 9, 13, 14 });
			per_shape_fn(glm::vec3(-1.0f, 0.0f, 0.0f)); 
			cell_locations = { { -1.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f } };
			break;
		default:
			throw std::runtime_error("Invalid TetrisPieceType");
		}	
	}

	glm::vec3 get_type_specific_offset() const { return type_specific_offset; }

	std::vector<glm::ivec2> get_cell_locations() const 
	{
		return get_cell_locations(get_transform());
	}

	std::vector<glm::ivec2> get_cell_locations(const glm::mat4& transform) const 
	{
		std::vector<glm::ivec2> result;
		for (auto& cell : cell_locations)
		{
			const auto transformed_cell = transform * glm::vec4(cell, 0.0f, 1.0f);
			result.emplace_back(std::round(transformed_cell.x), std::round(transformed_cell.y));
		}
		return result;
	}

private:
	TetrisPieceType type;
	// some shapes namely I and O have a center of rotation that's not directly on a cell
	glm::vec3 type_specific_offset{};

	// <x, y> location of the object == <0, 0>
	std::vector<glm::vec2> cell_locations;
};

class Application : public IApplication
{
public:
	Application(GameEngineT& engine) : 
		engine(engine)
	{
	}

	virtual void on_tick(float delta) override
	{
		static float elapsed_sec = 0;
		elapsed_sec += delta;

		// actual game tick has occurred
		if (elapsed_sec > 1.0f)
		{
			elapsed_sec = 0;

			const glm::mat4 new_transform = glm::translate(glm::mat4(1.0f), -Maths::up_vec) * 
				get_latest_piece().get_transform();
			try_transform_piece(new_transform);
		}
	}

	virtual void on_click(Object& object) override
	{
	}
	
	virtual void on_begin() override
	{
		engine.get_camera().look_at(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -50.0f));
		engine.get_light_source()->set_position(glm::vec3(0.0f, 0.0f, -100.0f));
		environment.set_position(glm::vec3(0.0f, 0.5f, 0.0f));
		engine.draw_object(environment);
		generate_next_piece();
	}

	virtual void on_key_press(int key, int scan_code, int action, int mode) override
	{
		const glm::mat4 curr_transform = get_latest_piece().get_transform();
		glm::mat4 transform;

		if (action == GLFW_PRESS)
		{
			switch (key)
			{
				case GLFW_KEY_LEFT:
					transform = glm::translate(glm::mat4(1.0f), glm::vec3(-1.0f, 0.0f, 0.0f)) * curr_transform;
					if (!check_for_collision(transform))
					{
						get_latest_piece().set_transform(transform);
					}
					break;
				case GLFW_KEY_RIGHT:
					transform = glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 0.0f, 0.0f)) * curr_transform;
					if (!check_for_collision(transform))
					{
						get_latest_piece().set_transform(transform);
					}
					break;
				case GLFW_KEY_UP:
					transform = curr_transform * glm::rotate(glm::mat4(1.0f), Maths::PI/2.0f, Maths::forward_vec);
					if (!check_for_collision(transform))
					{
						get_latest_piece().set_transform(transform);
					}
					break;
				case GLFW_KEY_DOWN:
					transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -1.0f, 0.0f)) * curr_transform;
					try_transform_piece(transform);
					break;
				case GLFW_KEY_SPACE:
					generate_next_piece();
					break;
				case GLFW_KEY_Z:
					draw_corresponding_cell_locations();
					break;
				default:
					break;
			}
		}
	}

	TetrisPiece& get_latest_piece()
	{
		return *pieces.back();
	}

private:
	void try_transform_piece(const glm::mat4& transform)
	{
		if (check_for_collision(transform))
		{
			on_collision();
		} else 
		{
			get_latest_piece().set_transform(transform);
		}
	}

	void generate_next_piece()
	{
		const int piece_type = Maths::RandomUniform(static_cast<int>(TetrisPieceType::I), static_cast<int>(TetrisPieceType::Z));
		const int color = Maths::RandomUniform<int>(0, standard_colors.size() - 1);
		// offsets due to some shapes such as L that can poke past the walls
		const glm::vec3 position = glm::vec3(Maths::RandomUniform(-width/2+2, width/2-3), height/2.0f, 0.0f);
		auto& new_piece = pieces.emplace_back(std::make_unique<TetrisPiece>(
			static_cast<TetrisPieceType>(piece_type), standard_colors[color]));
		new_piece->set_position(position + new_piece->get_type_specific_offset());
		engine.draw_object(*new_piece); 
	}

	bool check_for_collision(const glm::mat4& transform)
	{
		// check for collision with other pieces
		const auto cell_locations = get_latest_piece().get_cell_locations(transform);
		for (glm::ivec2 cell : cell_locations)
		{
			if (filled_spots.find(cell) != filled_spots.end())
			{
				return true;
			}
		}

		// check for collision with environment
		for (glm::ivec2 cell : cell_locations)
		{
			if (cell.x <= -width/2 || cell.x >= width/2 || cell.y <= -height/2)
			{
				return true;
			}
		}

		return false;
	}

	void on_collision()
	{
		// move piece to filled spots and generate new piece
		for (glm::ivec2 cell : get_latest_piece().get_cell_locations())
		{
			filled_spots.insert(cell);
		}

		// check for full rows


		// generate new piece
		generate_next_piece();
	}

	void draw_corresponding_cell_locations()
	{
		if (temporary_objects.empty())
		{
			for (auto& piece : pieces)
			{
				for (auto& cell : piece->get_cell_locations())
				{
					auto& obj = engine.spawn_object<Object>(ShapeFactory::generate_cube());
					obj.set_position(glm::vec3(cell, -0.5f));
					temporary_objects.push_back(obj.get_id());
				}
			}
		} else 
		{
			for (auto id : temporary_objects)
			{
				engine.delete_object(id);
			}

			temporary_objects.clear();
		}
	}

	GameEngineT& engine;
	std::vector<std::unique_ptr<TetrisPiece>> pieces;
	Environment environment;

	std::unordered_set<glm::ivec2> filled_spots;
	std::vector<uint64_t> temporary_objects;
};

int main()
{
	try {
		bool restart_signal = false;
		do {
			// seems like glfw window must be on main thread otherwise it wont work, 
			// therefore engine should always be on its own thread
			restart_signal = false;
			Config config(Utility::get().get_top_level_path().string() + "/configs/default.yaml");
			if (config.enable_logging())
			{
				Utility::get().enable_logging();
			}
			
			App::Window window;
			window.open(config.get_window_pos().first, config.get_window_pos().second);

			GameEngineT engine([&restart_signal](){restart_signal=true;}, window);

			Application app(engine);
			engine.set_application(&app);
			engine.run();
		} while (restart_signal);
    } catch (const std::exception& e) {
		fmt::print(fg(fmt::color::red), "Exception Thrown!: {}\n", e.what());
        return EXIT_FAILURE;
	} catch (...) {
		fmt::print(fg(fmt::color::red), "Exception Thrown!: UNKNOWN\n");
        return EXIT_FAILURE;
	}
}