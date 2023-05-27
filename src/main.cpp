#include "game_engine.hpp"
#include "graphics_engine/graphics_engine.hpp"
#include "window.hpp"
#include "config.hpp"
#include "utility.hpp"
#include "shapes/shape_factory.hpp"

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
			restart_signal = false;
			
			Config config(Utility::get().get_top_level_path().string() + "/configs/default.yaml");
			if (config.enable_logging())
			{
				Utility::get().enable_logging();
			}
			
			App::Window window;
			window.open(config.get_window_pos().first, config.get_window_pos().second);
			// seems like glfw window must be on main thread otherwise it wont work, 
			// therefore engine should always be on its own thread
			GameEngine<GraphicsEngine> engine([&restart_signal](){restart_signal=true;}, window);
			auto floor_shape =ShapeFactory::cube();
			Material floor_material{};
			floor_material.material_data.shininess = 1.0f;
			floor_shape->set_material(floor_material);
			auto& floor = engine.spawn_object<Object>(std::move(floor_shape));
			floor.set_scale(glm::vec3(100.0f, 0.1f, 100.0f));
			floor.set_position(glm::vec3(0.0f, -0.05f, 0.0f));
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