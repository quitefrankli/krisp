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
#include <imgui.h>

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
		transform = glm::translate(glm::mat4(1.0f), glm::vec3(-width/2-0.5f, 0.0f, 0.0f));
		transform = glm::scale(transform, glm::vec3(1.0f, height, 3.0f));
		material.material_data.diffuse = glm::vec3(0.2f, 0.3f, 0.2f);
		left_wall.set_material(material);
		left_wall.transform_vertices(transform);

		Shape right_wall = Shapes::Cube();
		transform = glm::translate(glm::mat4(1.0f), glm::vec3(width/2+0.5f, 0.0f, 0.0f));
		transform = glm::scale(transform, glm::vec3(1.0f, height, 3.0f));
		right_wall.set_material(material);
		right_wall.transform_vertices(transform);

		Shape back_wall = Shapes::Cube();
		transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 2.0f));
		transform = glm::scale(transform, glm::vec3(width+2, height, 1.0f));
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
	TetrisPiece(TetrisPieceType type) :
		type(type)
	{
		const auto per_shape_fn = [&](const glm::vec3& translation, const glm::vec3& color)
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
			per_shape_fn(glm::vec3(-1.5f, 0.5f, 0.0f), standard_colors[0]);
			cell_locations = { { -1.5f, 0.5f }, { -0.5f, 0.5f }, { 0.5f, 0.5f }, { 1.5f, 0.5f } };
			type_specific_offset = glm::vec3(0.5f, 0.5f, 0.0f);
			break;
		case TetrisPieceType::J:
			shapes = generate_shapes({ 8, 12, 13, 14 });
			per_shape_fn(glm::vec3(-1.0f, 0.0f, 0.0f), standard_colors[1]);
			cell_locations = { { -1.0f, 1.0f }, { -1.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f } };
			break;
		case TetrisPieceType::L:
			shapes = generate_shapes({ 10, 12, 13, 14 });
			per_shape_fn(glm::vec3(-1.0f, 0.0f, 0.0f), standard_colors[2]); 
			cell_locations = { { -1.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f } };
			break;
		case TetrisPieceType::O:
			shapes = generate_shapes({ 8, 9, 12, 13 });
			per_shape_fn(glm::vec3(-0.5f, -0.5f, 0.0f), standard_colors[3]); 
			cell_locations = { { -0.5f, 0.5f }, { 0.5f, 0.5f }, { -0.5f, -0.5f }, { 0.5f, -0.5f } };
			type_specific_offset = glm::vec3(0.5f, 0.5f, 0.0f);
			break;
		case TetrisPieceType::S:
			shapes = generate_shapes({ 9, 10, 12, 13 });
			per_shape_fn(glm::vec3(-1.0f, 0.0f, 0.0f), standard_colors[4]); 
			cell_locations = { { -1.0f, 0.0f }, { 0.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f } };
			break;
		case TetrisPieceType::T:
			shapes = generate_shapes({ 9, 12, 13, 14 });
			per_shape_fn(glm::vec3(-1.0f, 0.0f, 0.0f), standard_colors[5]); 
			cell_locations = { { -1.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f } };
			break;
		case TetrisPieceType::Z:
			shapes = generate_shapes({ 8, 9, 13, 14 });
			per_shape_fn(glm::vec3(-1.0f, 0.0f, 0.0f), standard_colors[6]); 
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

class TetrisGui : public GuiWindow<GameEngineT>, public GuiPhotoBase
{
public:
	void draw() override
	{
		ImGui::Begin("Tetris");

		ImGui::Text("Level: %d, Score: %d", level, score);
		restart = ImGui::Button("New Game");
		if (!game_over)
		{
			ImGui::SameLine();
			pause_unpause_toggled = ImGui::Button(paused ? "Unpause" : "Pause");
		} else
		{
			ImGui::Text("Game Over!");
		}

		ImGui::Text("Next Piece");
		GuiPhotoBase::draw();

		ImGui::End();
	}

	void process(GameEngineT& engine) override
	{
		if (pause_unpause_toggled)
		{
			paused = !paused;
		}
	}

	int score = 0;
	int level = 0;
	bool restart = false;
	bool paused = false;
	bool game_over = false;

private:
	bool pause_unpause_toggled = false;
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
		if (gui->restart)
		{
			gui->restart = false;
			restart();
			return;
		}

		if (gui->paused)
		{
			return;
		}

		static float elapsed_sec = 0;
		elapsed_sec += delta;

		// actual game tick has occurred
		if (elapsed_sec > period)
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
		gui = &engine.get_gui_manager().spawn_gui<TetrisGui>();
		engine.get_camera().look_at(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -50.0f));
		engine.get_light_source()->set_position(glm::vec3(0.0f, 0.0f, -100.0f));
		environment.set_position(glm::vec3(-0.5f, 0.5f, 0.0f));
		engine.draw_object(environment);
		generate_next_piece();
		engine.get_camera().set_orthographic_projection(glm::vec2(-22.0f, 22.0f));
		engine.get_camera().toggle_projection();

		Utility::get().set_appname_for_path("tetris");
		main_theme = std::make_unique<AudioSource>(engine.get_audio_engine().create_source());
		main_theme->set_audio((Utility::get().get_app_audio_path() / "music.wav").string());
		main_theme->set_gain(0.2f);
		main_theme->set_loop(true);
		main_theme->play();

		clear_line_fx = std::make_unique<AudioSource>(engine.get_audio_engine().create_source());
		clear_line_fx->set_audio((Utility::get().get_app_audio_path() / "clear.wav").string());
		clear_line_fx->set_gain(0.5f);
	}

	virtual void on_key_press(int key, int scan_code, int action, int mode) override
	{
		if (gui->paused)
		{
			return;
		}

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
					transform = curr_transform * glm::rotate(glm::mat4(1.0f), -Maths::PI/2.0f, Maths::forward_vec);
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
					while (try_transform_piece(glm::translate(glm::mat4(1.0f), -Maths::up_vec) * get_latest_piece().get_transform()));
					break;
				case GLFW_KEY_Z:
					break;
				default:
					break;
			}
		}
	}

	TetrisPiece& get_latest_piece()
	{
		return *current_piece;
	}

private:
	bool try_transform_piece(const glm::mat4& transform)
	{
		if (check_for_collision(transform))
		{
			on_collision();
			return false;
		} else 
		{
			get_latest_piece().set_transform(transform);
			return true;
		}
	}

	void generate_next_piece()
	{
		assert(!current_piece);

		const auto generate_new_piece = [&]()
		{
			const int piece_type = Maths::RandomUniform(static_cast<int>(TetrisPieceType::I), static_cast<int>(TetrisPieceType::Z));
			// offsets due to some shapes such as L that can poke past the walls
			auto* new_piece = &engine.spawn_object<TetrisPiece>(static_cast<TetrisPieceType>(piece_type));
			new_piece->set_visibility(false); // hide it from the main rasterization renderer (can be made simpler with a scene system)
			new_piece->set_scale(glm::vec3(3.0f)); // make it larger so it looks normal size in preview
			return new_piece;
		};

		if (!next_piece)
		{
			next_piece = generate_new_piece();
		}

		current_piece = next_piece;
		const glm::vec3 position = glm::vec3(Maths::RandomUniform(-width/2+2, width/2-3), height/2.0f, 0.0f);
		current_piece->set_position(position + current_piece->get_type_specific_offset());
		current_piece->set_visibility(true);
		current_piece->set_scale(glm::vec3(1.0f)); // reset scale to normal size
		next_piece = generate_new_piece();
		engine.preview_objs_in_gui({ next_piece }, dynamic_cast<GuiPhotoBase&>(*gui));

		// if we are already colliding with another piece then this is game over
		if (check_for_collision(get_latest_piece().get_transform()))
		{
			game_over();
		}

		++piece_count;
		gui->level = get_level();
		period = std::clamp(1.0f - get_level() * 0.04f, 0.1f, 1.0f);
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
			if (cell.x < -width/2 || cell.x >= width/2 || cell.y <= -height/2)
			{
				return true;
			}
		}

		return false;
	}

	void restart()
	{
		// destroy remnants of previous game
		for (auto& row : entrenched_cells)
		{
			for (auto* cell : row)
			{
				engine.delete_object(cell->get_id());
			}
			row.clear();
		}

		filled_spots.clear();
		engine.delete_object(get_latest_piece().get_id());
		current_piece = nullptr;

		// prepare for next game
		generate_next_piece();
		main_theme->stop();
		main_theme->play();
		gui->score = 0;
		gui->game_over = false;
		gui->paused = false;
		piece_count = 0;
	}

	void on_collision()
	{
		// move piece to filled spots and generate new piece
		for (glm::ivec2 cell : get_latest_piece().get_cell_locations())
		{
			filled_spots.insert(cell);
		}

		// hacky workaround, replace all cells with a new object
		// we really should have all cells be objects NOT shapes
		for (auto cell : get_latest_piece().get_cell_locations())
		{
			Shape new_cell = ShapeFactory::generate_cube();
			new_cell.set_material(get_latest_piece().get_shapes().back().get_material());
			auto& obj = engine.spawn_object<Object>(std::move(new_cell));
			obj.set_position(glm::vec3(cell, 0.0f));
			entrenched_cells[cell.y+height/2].push_back(&obj);
		}
		engine.delete_object(get_latest_piece().get_id());
		current_piece = nullptr;

		// check for full rows, go from bottom to top
		int rows_cleared = 0;
		for (int row = 0; row < entrenched_cells.size();)
		{
			if (clear_row_if_necessary(row))
			{
				++rows_cleared;
				// move all rows above down
				for (int row_above = row+1; row_above < entrenched_cells.size(); row_above++)
				{
					for (auto* cell : entrenched_cells[row_above])
					{
						filled_spots.erase(glm::ivec2(
							std::round(cell->get_position().x), 
							std::round(cell->get_position().y)));
						filled_spots.insert(glm::ivec2(
							std::round(cell->get_position().x), 
							std::round(cell->get_position().y) - 1));
						cell->set_position(cell->get_position() - Maths::up_vec);
					}
					entrenched_cells[row_above-1] = std::move(entrenched_cells[row_above]);
				}
			} else
			{
				++row;
			}
		}

		if (rows_cleared)
		{
			clear_line_fx->play();
			gui->score += rows_cleared * rows_cleared * 100;
		}

		// generate new piece
		generate_next_piece();
	}

	bool clear_row_if_necessary(int row)
	{
		if (entrenched_cells[row].size() == width)
		{
			for (auto* cell : entrenched_cells[row])
			{
				const auto pos = cell->get_position();
				filled_spots.erase(glm::ivec2(std::round(pos.x), std::round(pos.y)));
				engine.delete_object(cell->get_id());
			}
			entrenched_cells[row].clear();
			return true;
		}
		return false;
	}

	int get_level() const 
	{
		return std::min(piece_count / 10 + 1, 20);
	}

	void game_over()
	{
		main_theme->stop();
		gui->game_over = true;
		// can simulate game over with a paused game that can only be resumed on new game
		gui->paused = true;
	}

private:
	GameEngineT& engine;
	TetrisPiece* next_piece = nullptr;
	TetrisPiece* current_piece = nullptr;
	Environment environment;
	TetrisGui* gui = nullptr;
	int piece_count = 0;
	float period = 0.5f; // periodicity of piece movement in sec

	std::array<std::vector<Object*>, height+2> entrenched_cells;
	std::unordered_set<glm::ivec2> filled_spots;
	std::unique_ptr<AudioSource> main_theme;
	std::unique_ptr<AudioSource> clear_line_fx;
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

	fmt::print(fg(fmt::color::green), "Clean shutdown success!\n");
}