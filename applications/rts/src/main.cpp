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
	bool restart_signal = false;
	do {
		// seems like glfw window must be on main thread otherwise it wont work, 
		// therefore engine should always be on its own thread
		restart_signal = false;
		// TODO: use configparser library for this
		if (argc == 2)
		{
			Config::initialise_global_config(Utility::get().get_config_path().string() + "/" + argv[1]);
		} else
		{
			Config::initialise_global_config(Utility::get().get_config_path().string() + "/default.yaml");
		}

		if (Config::enable_logging())
		{
			Utility::get().enable_logging();
		}
		
		App::Window window;
		window.open(Config::get_window_pos().first, Config::get_window_pos().second);

		GameEngineT engine([&restart_signal](){restart_signal=true;}, window);

		Application app;
		engine.set_application(&app);
		auto floor_shape =ShapeFactory::cube();
		Material floor_material{};
		floor_material.material_data.shininess = 1.0f;
		floor_shape->set_material(floor_material);
		auto& floor = engine.spawn_object<Object>(std::move(floor_shape));
		floor.set_scale(glm::vec3(100.0f, 0.1f, 100.0f));
		floor.set_position(glm::vec3(0.0f, -0.05f, 0.0f));
		auto& floating_obj1 = engine.spawn_object<Object>(ShapeFactory::cube());
		floating_obj1.set_position(glm::vec3(0.0f, 1.5f, 0.0f));
		floating_obj1.set_rotation(glm::angleAxis(glm::radians(45.0f), glm::vec3(1.0f, 0.0f, 0.0f)));
		engine.get_ecs().add_clickable_entity(floating_obj1.get_id());
		engine.get_ecs().add_collider(floating_obj1.get_id(), std::make_unique<SphereCollider>());
		engine.run();
	} while (restart_signal);

	fmt::print(fg(fmt::color::green), "Clean shutdown success!\n");
}