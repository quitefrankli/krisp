#include "piece.hpp"

#include <game_engine.hpp>
#include <graphics_engine/graphics_engine.hpp>
#include <resource_loader/resource_loader.hpp>
#include <renderable/mesh_factory.hpp>
#include <renderable/material_factory.hpp>
#include <iapplication.hpp>
#include <utility.hpp>
#include <camera.hpp>
#include <config.hpp>

#include <fmt/core.h>
#include <fmt/color.h>
#include <glm/gtx/string_cast.hpp>
#include <imgui.h>

#include <GLFW/glfw3.h> // for key macros

#include <iostream>


constexpr int height = 20;
constexpr int width = 10;


class TetrisGui : public GuiWindow, public GuiPhotoBase
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

	void process(GameEngine& engine) override
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
	Application(GameEngine& engine) : 
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
		auto light_source_id = engine.get_ecs().get_global_light_source();
		engine.get_ecs().get_object(light_source_id).set_position(glm::vec3(-4.0f, 20.0f, -50.0f));
		spawn_environment();
		generate_next_piece();
		// engine.get_camera().set_orthographic_projection(glm::vec2(-22.0f, 22.0f));
		// engine.get_camera().toggle_projection();

		// Utility::get().set_appname_for_path("tetris");
		// main_theme = std::make_unique<AudioSource>(engine.get_audio_engine().create_source());
		// main_theme->set_audio((Utility::get().get_app_audio_path() / "music.wav").string());
		// main_theme->set_gain(0.2f);
		// main_theme->set_loop(true);
		// main_theme->play();

		// clear_line_fx = std::make_unique<AudioSource>(engine.get_audio_engine().create_source());
		// clear_line_fx->set_audio((Utility::get().get_app_audio_path() / "clear.wav").string());
		// clear_line_fx->set_gain(0.5f);
	}
	virtual void on_key_press(const KeyInput& key_input) override
	{
		if (gui->paused)
		{
			return;
		}

		const glm::mat4 curr_transform = get_latest_piece().get_transform();
		glm::mat4 transform;

		if (key_input.action == EInputAction::PRESS)
		{
			switch (key_input.key)
			{
				case GLFW_KEY_LEFT:
					transform = glm::translate(Maths::identity_mat, -Maths::right_vec) * curr_transform;
					if (!check_for_collision(transform))
					{
						get_latest_piece().set_transform(transform);
					}
					break;
				case GLFW_KEY_RIGHT:
					transform = glm::translate(Maths::identity_mat, Maths::right_vec) * curr_transform;
					if (!check_for_collision(transform))
					{
						get_latest_piece().set_transform(transform);
					}
					break;
				case GLFW_KEY_UP:
					transform = curr_transform * glm::rotate(Maths::identity_mat, -Maths::PI/2.0f, Maths::forward_vec);
					if (!check_for_collision(transform))
					{
						get_latest_piece().set_transform(transform);
					}
					break;
				case GLFW_KEY_DOWN:
					try_transform_piece(glm::translate(Maths::identity_mat, -Maths::up_vec) * curr_transform);
					break;
				case GLFW_KEY_SPACE:
					transform = glm::translate(Maths::identity_mat, -Maths::up_vec);
					while (try_transform_piece(transform * get_latest_piece().get_transform()));
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
	// includes floor and walls
	void spawn_environment()
	{
		auto cube_renderable = Renderable::make_default(MeshFactory::cube_id());

		auto& root = engine.spawn_object<Object>(cube_renderable);
		root.set_visibility(false);

		auto& floor = engine.spawn_object<Object>(cube_renderable);
		floor.set_position(glm::vec3(0.0f, -10.75f, 0.0f));
		floor.set_scale(glm::vec3(width*2, 1.5f, 3.0f));
		floor.attach_to(&root);

		auto& left_wall = engine.spawn_object<Object>(cube_renderable);
		left_wall.set_position(glm::vec3(-width/2-0.5f, 0.0f, 0.0f));
		left_wall.set_scale(glm::vec3(1.0f, height, 3.0f));
		left_wall.attach_to(&root);

		auto& right_wall = engine.spawn_object<Object>(cube_renderable);
		right_wall.set_position(glm::vec3(width/2+0.5f, 0.0f, 0.0f));
		right_wall.set_scale(glm::vec3(1.0f, height, 3.0f));
		right_wall.attach_to(&root);

		auto& back_wall = engine.spawn_object<Object>(cube_renderable);
		back_wall.set_position(glm::vec3(0.0f, 0.0f, 2.0f));
		back_wall.set_scale(glm::vec3(width+2, height, 1.0f));
		back_wall.attach_to(&root);

		root.set_position(glm::vec3(-0.5f, 0.5f, 0.0f));
	}

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
			auto* new_piece = &engine.spawn_object<TetrisPiece>(static_cast<TetrisPieceType>(piece_type), engine);
			new_piece->set_position(glm::vec3(0.0f, 0.0f, 10.0f)); // position out of view
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
		auto& latest_piece = get_latest_piece();
		// move piece to filled spots and generate new piece
		const auto cell_locations = latest_piece.get_cell_locations();
		for (unsigned i = 0; i < cell_locations.size(); ++i)
		{
			auto* cell = engine.get_object(latest_piece.get_cells()[i]);
			const auto& loc = cell_locations[i];
			filled_spots.insert(loc);
			entrenched_cells[loc.y+height/2].push_back(cell);
		}

		auto* parent = engine.get_object(latest_piece.get_id());
		parent->detach_all_children();
		engine.delete_object(latest_piece.get_id());
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
			// clear_line_fx->play();
			gui->score += rows_cleared * rows_cleared * 100;
		}

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
	GameEngine& engine;
	TetrisPiece* next_piece = nullptr;
	TetrisPiece* current_piece = nullptr;
	TetrisGui* gui = nullptr;
	int piece_count = 0;
	float period = 0.5f; // periodicity of piece movement in sec

	std::array<std::vector<Object*>, height+2> entrenched_cells;
	std::unordered_set<glm::ivec2> filled_spots;
	std::unique_ptr<AudioSource> main_theme;
	std::unique_ptr<AudioSource> clear_line_fx;
};

int main(int argc, char* argv[])
{
	const std::string config_path = argc == 2 ? argv[1] : "default.yaml";
	Config::initialise_global_config(Utility::get_config_path().string() + "/" + config_path);
	
	if (Config::enable_logging())
	{
		Utility::enable_logging();
	}
		
	{
		App::Window window;
		window.open(Config::get_window_pos().first, Config::get_window_pos().second);
		GameEngine engine(window);

		Application app(engine);
		engine.set_application(&app);

		engine.run();
	}

	fmt::print(fg(fmt::color::green), "Clean shutdown success!\n");
}