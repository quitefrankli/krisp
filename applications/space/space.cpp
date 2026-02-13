#include "scenarios/stable_solar_system.hpp"

#include <game_engine.hpp>
#include <window.hpp>
#include <config.hpp>
#include <utility.hpp>

#include <fmt/core.h>
#include <fmt/color.h>


int main(int argc, char* argv[])
{
#ifdef _DEBUG
	fmt::print(fg(fmt::color::cyan), "Debug Mode\n");
#else
	fmt::print(fg(fmt::color::cyan), "Release Mode\n");
#endif

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

		engine.get_ecs().get_gravity_system().set_gravity_type(GravitySystem::GravityType::TRUE);
		Scenarios::setup_orbital_system(engine);

		engine.run();
	}

	fmt::print(fg(fmt::color::green), "Clean shutdown success!\n");
}