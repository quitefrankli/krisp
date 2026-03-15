#include "scenarios/stable_solar_system.hpp"

#include <game_engine.hpp>
#include <iapplication.hpp>
#include <window.hpp>
#include <config.hpp>
#include <utility.hpp>
#include <objects/cubemap.hpp>

#include <fmt/core.h>
#include <fmt/color.h>


int main(int argc, char* argv[])
{
	Config::init(PROJECT_NAME);
	auto engine = GameEngine::create<DummyApplication>();

	engine.spawn_object<CubeMap>();

	engine.get_ecs().get_gravity_system().set_gravity_type(GravitySystem::GravityType::TRUE);
	Scenarios::setup_orbital_system(engine);

	engine.run();
}