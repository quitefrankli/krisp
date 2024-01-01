#include "tile_system.hpp"

#include <game_engine.hpp>
#include <graphics_engine/graphics_engine.hpp>
#include <iapplication.hpp>
#include <resource_loader.hpp>
#include <utility.hpp>
#include <camera.hpp>
#include <config.hpp>
#include <shapes/shape_factory.hpp>

#include <fmt/core.h>
#include <fmt/color.h>
#include <glm/gtx/string_cast.hpp>
#include <imgui.h>

#include <GLFW/glfw3.h> // for key macros

#include <iapplication.hpp>
#include <game_engine.hpp>

#include <iostream>

using GameEngineT = GameEngine<GraphicsEngine>;


class Application : public IApplication
{
public:
	// in seconds
	virtual void on_tick(float delta) {};
	virtual void on_click(Object& object) {};
	virtual void on_begin() {};
	virtual void on_key_press(int key, int scan_code, int action, int mode) {};
};

int main(int argc, char* argv[])
{
	Config::initialise_global_config(Utility::get().get_config_path().string() + "/default.yaml");
	App::Window window;
	window.open(Config::get_window_pos().first, Config::get_window_pos().second);
	GameEngineT engine(window);
	Utility::get().enable_logging();

	Application app;
	engine.set_application(&app);

	Tile::tile_object_spawner = [&engine](const TileCoord& coord)
	{
		Object& obj = engine.spawn_object<Object>(ShapeFactory::cube());
		obj.set_position(glm::vec3(coord.x, 0.5f, coord.y));
		obj.set_scale(glm::vec3(0.95f, 0.95f, 0.95f));
	};
	TileSystem tile_system;

	engine.run();
	fmt::print(fg(fmt::color::green), "Clean shutdown success!\n");
}