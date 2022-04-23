#include <game_engine.hpp>
#include <utility.hpp>
#include <iapplication.hpp>

#include <fmt/core.h>
#include <fmt/color.h>

#include <iostream>


class Application : public DummyApplication
{
public:
	Application(GameEngine& engine) : engine(engine) 
	{
		engine.set_application(this);
	}

	const float max_time = 5.0f; // in secs
	float elapsed = 0.0f;
	GameEngine& engine;

	virtual void on_tick(float delta) override
	{
		elapsed+=delta;
		if (elapsed > max_time)
		{
			const auto& counter = engine.get_gui_manager().fps_counter; 
			fmt::print("Avg FPS={}, Avg TPS={}\n", counter.fps, counter.tps);
			engine.shutdown();
		}
	}
};

int main()
{
	std::cout << "Running Benchmark!\n";
	try {
		bool restart_signal = false;
		do {
			// seems like glfw window must be on main thread otherwise it wont work, 
			// therefore engine should always be on its own thread
			restart_signal = false;
			GameEngine engine([&restart_signal](){restart_signal=true;});
			const float limit = 100.0f;
			const int num_objs = 900;
			for (float y = -30; y < 30; y+=2)
			{
				for (float x = -30; x < 30; x+=2)
				{
					engine.spawn_object<Sphere>().set_position(glm::vec3(x, y, 75));
				}
			}
			Application benchmarker(engine);
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