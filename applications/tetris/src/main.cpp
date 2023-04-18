#include <game_engine.hpp>
#include <graphics_engine/graphics_engine.hpp>
#include <iapplication.hpp>
#include <resource_loader.hpp>
#include <utility.hpp>
#include <camera.hpp>
#include <objects/light_source.hpp>
#include <shapes/shapes.hpp>
#include <config.hpp>

#include <fmt/core.h>
#include <fmt/color.h>
#include <glm/gtx/string_cast.hpp>

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

		Shape floor = Shapes::Cube();
		floor.set_color(glm::vec3(0.5f, 0.3f, 0.2f));
		transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -5.75f, 0.0f));
		transform = glm::scale(transform, glm::vec3(width*2, 1.5f, 3.0f));
		floor.transform_vertices(transform);

		Shape left_wall = Shapes::Cube();
		transform = glm::translate(glm::mat4(1.0f), glm::vec3(-5.0f, 0.0f, 0.0f));
		transform = glm::scale(transform, glm::vec3(1.0f, height, 3.0f));
		left_wall.transform_vertices(transform);
		left_wall.set_color(glm::vec3(0.2f, 0.3f, 0.2f));

		Shape right_wall = Shapes::Cube();
		transform = glm::translate(glm::mat4(1.0f), glm::vec3(5.0f, 0.0f, 0.0f));
		transform = glm::scale(transform, glm::vec3(1.0f, height, 3.0f));
		right_wall.transform_vertices(transform);
		right_wall.set_color(glm::vec3(0.2f, 0.3f, 0.2f));

		Shape back_wall = Shapes::Cube();
		transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 2.0f));
		transform = glm::scale(transform, glm::vec3(10.0f, 10.0f, 1.0f));
		back_wall.transform_vertices(transform);
		back_wall.set_color(glm::vec3(0.2f, 0.3f, 0.2f));

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

class TetrisPiece : public Object
{
public:
	TetrisPiece(TetrisPieceType type) :
		type(type)
	{
		switch (type)
		{
		case TetrisPieceType::I:
			shapes = generate_shapes({ 12, 13, 14, 15 });
			// move so that origin matches the rotation point
			std::for_each(shapes.begin(), shapes.end(), [](Shape& shape) { 
				shape.translate_vertices(glm::vec3(-1.5f, 0.5f, 0.0f)); 
				shape.set_color(glm::vec3(0.0f, 1.0f, 1.0f));
			});
			cell_locations = { { -1.5f, 0.5f }, { -0.5f, 0.5f }, { 0.5f, 0.5f }, { 1.5f, 0.5f } };
			break;
		case TetrisPieceType::J:
			shapes = generate_shapes({ 8, 12, 13, 14 });
			// move so that origin matches the rotation point
			std::for_each(shapes.begin(), shapes.end(), [](auto& shape) { shape.translate_vertices(glm::vec3(-1.0f, 0.0f, 0.0f)); });
			cell_locations = { { -1.0f, 1.0f }, { -1.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f } };
			break;
		}
	}

private:
	TetrisPieceType type;

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

			// get_latest_piece().set_position(get_latest_piece().get_position() + glm::vec3(0.0f, -1.0f, 0.0f));
		}
	}

	virtual void on_click(Object& object) override
	{
	}
	
	virtual void on_begin() override
	{
		engine.get_camera().look_at(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -50.0f));
		engine.get_light_source()->set_position(glm::vec3(0.0f, 0.0f, -100.0f));

		engine.draw_object(environment);

		pieces.emplace_back(TetrisPieceType::I);
		pieces.emplace_back(TetrisPieceType::J);
		pieces[0].set_position(glm::vec3(-5.0f, 5.0f, 0.0f));
		pieces[1].set_position(glm::vec3(5.0f, 5.0f, 0.0f));
		engine.draw_object(pieces[0]);
		engine.draw_object(pieces[1]);
	}

	virtual void on_key_press(int key, int scan_code, int action, int mode) override
	{
		static const auto left_rotation = glm::angleAxis(Maths::PI / 2.0f, Maths::forward_vec);
		static const auto right_rotation = glm::angleAxis(-Maths::PI / 2.0f, Maths::forward_vec);

		if (action == GLFW_PRESS)
		{
			switch (key)
			{
				case GLFW_KEY_LEFT:
					get_latest_piece().set_position(get_latest_piece().get_position() + glm::vec3(-1.0f, 0.0f, 0.0f));
					break;
				case GLFW_KEY_RIGHT:
					get_latest_piece().set_position(get_latest_piece().get_position() + glm::vec3(1.0f, 0.0f, 0.0f));
					break;
				case GLFW_KEY_UP:
					get_latest_piece().set_rotation(left_rotation * get_latest_piece().get_rotation());
					break;
				case GLFW_KEY_DOWN:
					// get_latest_piece().set_rotation(right_rotation * get_latest_piece().get_rotation());
					break;
				case GLFW_KEY_SPACE:
					break;
				default:
					break;
			}
		}
	}

	TetrisPiece& get_latest_piece()
	{
		return pieces.back();
	}

private:
	bool check_for_collision()
	{
		// check for collision with other pieces


		// check for collision with environment

	}

	GameEngineT& engine;
	std::vector<TetrisPiece> pieces;
	Environment environment;
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