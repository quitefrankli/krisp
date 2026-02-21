#include "pieces.hpp"
#include "board.hpp"
#include "state_machine.hpp"

#include <game_engine.hpp>
#include <window.hpp>
#include <config.hpp>
#include <utility.hpp>
#include <iapplication.hpp>
#include <camera.hpp>
#include <objects/cubemap.hpp>
#include <renderable/mesh_factory.hpp>
#include <renderable/material_factory.hpp>
#include <entity_component_system/light_source.hpp>

#include <fmt/core.h>
#include <fmt/color.h>

class Application : public IApplication
{
public:
	Application(GameEngine& engine) :
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
	}

	virtual void on_key_press(const KeyInput& key_input) override
	{

	}

private:
	GameEngine& engine;
	Board board;
	Tile* active_tile = nullptr;
	State* state = State::initial.get();
};

int main(int argc, char* argv[])
{
	Config::init(PROJECT_NAME);
	{
		App::Window window;
		window.open(Config::get_window_pos().first, Config::get_window_pos().second);
		GameEngine engine(window);

		auto& ecs = engine.get_ecs();

		// Add skybox
		engine.spawn_object<CubeMap>();

		// Add light source
		auto& light_source = engine.spawn_object<Object>(Renderable{
			.mesh_id = MeshFactory::sphere_id(),
			.material_ids = { MaterialFactory::fetch_preset(EMaterialPreset::LIGHT_SOURCE) },
			.pipeline_render_type = ERenderType::COLOR,
			.casts_shadow = false
		});
		light_source.set_position(glm::vec3(0.0f, 10.0f, 0.0f));
		light_source.set_scale(glm::vec3(2.0f));
		
		ecs.add_collider(light_source.get_id(), std::make_unique<SphereCollider>());
		ecs.add_clickable_entity(light_source.get_id());

		LightComponent light;
		light.intensity = 1.0f;
		light.color = glm::vec3(1.0f, 0.95f, 0.9f);
		ecs.add_light_source(light_source.get_id(), light);

		Application app(engine);
		engine.set_application(&app);
		engine.run();
	}

	fmt::print(fg(fmt::color::green), "Clean shutdown success!\n");
}