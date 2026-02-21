#include "scenarios/stable_solar_system.hpp"

#include <game_engine.hpp>
#include <window.hpp>
#include <config.hpp>
#include <utility.hpp>

#include <fmt/core.h>
#include <fmt/color.h>


int main(int argc, char* argv[])
{
	Config::init(PROJECT_NAME);
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