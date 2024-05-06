#include "game_engine.hpp"
#include "graphics_engine/graphics_engine.hpp"
#include "window.hpp"
#include "config.hpp"
#include "utility.hpp"
#include "renderable/mesh_factory.hpp"
#include "renderable/material_factory.hpp"

#include <fmt/core.h>
#include <fmt/color.h>

#include <iostream>
#include <thread>
#include <iomanip>


int main(int argc, char* argv[])
{
#ifdef NDEBUG
	fmt::print(fg(fmt::color::blue), "Release Mode\n");
#else
	fmt::print(fg(fmt::color::blue), "Debug Mode\n");
#endif
	const std::string config_path = argc == 2 ? argv[1] : "default.yaml";
	Config::initialise_global_config(Utility::get().get_config_path().string() + "/" + config_path);

	if (Config::enable_logging())
	{
		Utility::get().enable_logging();
	}
		
	{
		App::Window window;
		window.open(Config::get_window_pos().first, Config::get_window_pos().second);
		// seems like glfw window must be on main thread otherwise it wont work, 
		// therefore engine should always be on its own thread
		GameEngine engine(window);
		Renderable floor_renderable;
		floor_renderable.pipeline_render_type = EPipelineType::COLOR;
		floor_renderable.mesh_id = MeshFactory::cube_id();
		floor_renderable.material_ids = { MaterialFactory::fetch_preset(EMaterialPreset::DIFFUSE) };
		auto& floor = engine.spawn_object<Object>(floor_renderable);
		floor.set_scale(glm::vec3(100.0f, 0.1f, 100.0f));
		floor.set_position(glm::vec3(0.0f, -0.05f, 0.0f));

		auto& floating_obj1 = engine.spawn_object<Object>(Renderable::make_default(MeshFactory::cube_id()));
		floating_obj1.set_position(glm::vec3(0.0f, 1.5f, 0.0f));
		floating_obj1.set_rotation(glm::angleAxis(glm::radians(45.0f), glm::vec3(1.0f, 0.0f, 0.0f)));
		engine.get_ecs().add_clickable_entity(floating_obj1.get_id());
		engine.get_ecs().add_collider(floating_obj1.get_id(), std::make_unique<SphereCollider>());
		engine.run();
	}

	fmt::print(fg(fmt::color::green), "Clean shutdown success!\n");
}