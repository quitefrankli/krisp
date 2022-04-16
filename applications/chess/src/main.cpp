#include "pieces.hpp"
#include "board.hpp"

#include <game_engine.hpp>
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
	Application(GameEngine& engine) : 
		engine(engine),
		board(engine)
	{
	}

	virtual void on_tick(float delta) override
	{

	}

	virtual void on_click(Object& object) override
	{
		Piece* piece = dynamic_cast<Piece*>(&object);
		if (piece)
		{
			engine.delete_object(piece->get_id());
			return;
		}
		
		Tile* tile = dynamic_cast<Tile*>(&object);
		if (!tile)
		{
			return;
		}

		// we have already selected an active tile

		if (active_tile)
		{
			active_tile->highlight(false);
			active_tile = nullptr;
		}

		active_tile = tile;
		active_tile->highlight(true);
	}
	
	virtual void on_begin() override
	{
		engine.get_camera().look_at(glm::vec3(0.0f), glm::vec3(0.0f, 20.0f, -50.0f));
		engine.get_light_source()->set_position(glm::vec3(0.0f, 100.0f, 0.0f));
	}

private:
	GameEngine& engine;
	Board board;
	Tile* active_tile = nullptr;
};

int main()
{
	try {
		bool restart_signal = false;
		do {
			// seems like glfw window must be on main thread otherwise it wont work, 
			// therefore engine should always be on its own thread
			restart_signal = false;
			GameEngine engine([&restart_signal](){restart_signal=true;});

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