#include <game_engine.hpp>
#include <graphics_engine/graphics_engine.hpp>
#include <iapplication.hpp>
#include <window.hpp>
#include <config.hpp>
#include <utility.hpp>
#include <renderable/mesh_factory.hpp>
#include <renderable/material_factory.hpp>
#include <objects/cubemap.hpp>

#include <fmt/core.h>
#include <fmt/color.h>

#include <iostream>
#include <thread>
#include <iomanip>


int main(int argc, char* argv[])
{
	Config::init(PROJECT_NAME);
	{
		auto engine = GameEngine::create<DummyApplication>();
		engine.spawn_object<CubeMap>(); // background/horizon
		Renderable floor_renderable;
		floor_renderable.pipeline_render_type = ERenderType::COLOR;
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

		auto& light_source = engine.spawn_object<Object>(Renderable{
			.mesh_id = MeshFactory::sphere_id(),
			.material_ids = { MaterialFactory::fetch_preset(EMaterialPreset::LIGHT_SOURCE) },
			.pipeline_render_type = ERenderType::COLOR
		});
		light_source.set_position(glm::vec3(0.0f, 8.0f, 0.0f));
		LightComponent light_component{
			.intensity = 1.0f,
			.color = { 1.0f, 0.9f, 0.2f }
		};
		engine.get_ecs().add_light_source(light_source.get_id(), light_component);
		engine.get_ecs().add_clickable_entity(light_source.get_id());
		engine.get_ecs().add_collider(light_source.get_id(), std::make_unique<SphereCollider>());

		engine.run();
	}

	fmt::print(fg(fmt::color::green), "Clean shutdown success!\n");
}