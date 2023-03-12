#include "game_engine.hpp"
#include <graphics_engine/graphics_engine.hpp>
#include <window.hpp>

#include <fmt/core.h>
#include <fmt/color.h>

#include <iostream>
#include <thread>
#include <iomanip>


int main()
{
#ifdef NDEBUG
	fmt::print(fg(fmt::color::blue), "Release Mode\n");
#else
	fmt::print(fg(fmt::color::blue), "Debug Mode\n");
#endif
    try {
		bool restart_signal = false;
		do {
			// seems like glfw window must be on main thread otherwise it wont work, 
			// therefore engine should always be on its own thread
			restart_signal = false;
			App::Window window;
			GameEngine<GraphicsEngine> engine([&restart_signal](){restart_signal=true;}, window);
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