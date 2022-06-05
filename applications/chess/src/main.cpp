#include "chess_engine.hpp"
#include "pieces.hpp"
#include "board.hpp"
#include "state_machine.hpp"

#include <game_engine.hpp>
#include <graphics_engine/graphics_engine.hpp>
#include <iapplication.hpp>
#include <resource_loader.hpp>
#include <utility.hpp>
#include <camera.hpp>
#include <objects/light_source.hpp>

#include <fmt/core.h>
#include <fmt/color.h>
#include <glm/gtx/string_cast.hpp>

#include <iostream>

class Application : public IApplication
{
public:
	Application(GameEngineT& engine) : 
		engine(engine),
		board(engine)
	{
		State::board = &board;
		State::engine = &engine;
	}

	virtual void on_tick(float delta) override
	{

	}

	virtual void on_click(Object& object) override
	{
		state = state->process(object);
	}
	
	virtual void on_begin() override
	{
		engine.get_camera().look_at(glm::vec3(0.0f), glm::vec3(0.0f, 20.0f, -50.0f));
		engine.get_light_source()->set_position(glm::vec3(0.0f, 100.0f, 0.0f));
	}

private:
	GameEngineT& engine;
	Board board;
	Tile* active_tile = nullptr;
	State* state = State::initial.get();
};

int main()
{
	try {
		bool restart_signal = false;
		do {
			// seems like glfw window must be on main thread otherwise it wont work, 
			// therefore engine should always be on its own thread
			restart_signal = false;
			GameEngineT engine([&restart_signal](){restart_signal=true;});

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