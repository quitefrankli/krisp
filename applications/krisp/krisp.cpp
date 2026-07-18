#include <game_engine.hpp>
#include <camera.hpp>
#include <graphics_engine/graphics_engine.hpp>
#include <iapplication.hpp>
#include <window.hpp>
#include <config.hpp>
#include <utility.hpp>
#include <renderable/mesh_factory.hpp>
#include <renderable/material_factory.hpp>
#include <objects/cubemap.hpp>
#include <resource_loader/resource_loader.hpp>
#include <game_objects/player_character.hpp>
#include <entity_component_system/collider_ecs.hpp>

#include <fmt/core.h>
#include <fmt/color.h>

#include <iostream>
#include <thread>
#include <iomanip>
#include <ranges>
#include <stdexcept>


namespace
{
// These names are the only asset-specific part of the demo. Update them to
// match the supplied glTF character before running the application.
constexpr std::string_view player_model = "player.glb";
constexpr std::string_view idle_clip = "Idle";
constexpr std::string_view walk_clip = "Walk";

class PlayerDemoApplication : public IApplication
{
public:
	void on_begin(GameEngine& engine) override
	{
		auto model = ResourceLoader::load_model(engine.get_ecs(), Utility::get_model(player_model));
		auto mesh = std::ranges::find_if(model.meshes, [](const auto& candidate)
		{
			return candidate.skeleton_id.has_value();
		});
		if (mesh == model.meshes.end())
			throw std::runtime_error("Player model must contain a skinned mesh");
		const auto find_clip = [&](std::string_view name)
		{
			auto clip = std::ranges::find_if(model.animations, [&](const AnimationID id)
			{
				return engine.get_ecs().get_skeletal_animations().at(id).name == name;
			});
			if (clip == model.animations.end())
				throw std::runtime_error("Player model is missing required animation clip: " + std::string(name));
			return *clip;
		};

		PlayerDefinition definition;
		definition.idle_animation = find_clip(idle_clip);
		definition.walk_animation = find_clip(walk_clip);
		// Set these to the supplied rig's leg chain names to enable foot placement.
		definition.left_leg = { "LeftUpLeg", "LeftLeg", "LeftFoot" };
		definition.right_leg = { "RightUpLeg", "RightLeg", "RightFoot" };
		player = &engine.spawn_object<PlayerCharacter>(mesh->renderables, *mesh->skeleton_id, definition);
		player->set_transform(model.onload_transform.get_mat4() * mesh->transform.get_mat4());
		player->set_name("Player");

		auto& camera = engine.get_camera();
		camera.look_at(player->get_position() + definition.camera_focus_offset,
			player->get_position() + glm::vec3(0.0f, 2.0f, -5.0f));
		camera.focus_obj->attach_to(player);
		camera.focus_obj->set_relative_position(definition.camera_focus_offset);
		engine.set_camera_keyboard_navigation_enabled(false);
		engine.set_camera_orbit_with_right_mouse(true);
	}

	void on_pre_tick(GameEngine& engine, float delta) override
	{
		player->pre_update(engine.get_keyboard(), engine.get_camera(), engine.get_ecs(), delta);
	}

	void on_tick(GameEngine&, float) override {}

	void on_post_tick(GameEngine& engine, float) override
	{
		player->post_animation_update(engine.get_ecs());
	}

	void on_click(GameEngine&, Object&) override {}
	void on_key_press(GameEngine&, const KeyInput&) override {}

private:
	PlayerCharacter* player = nullptr;
};
}


int main(int argc, char* argv[])
{
	Config::init(PROJECT_NAME);
	// auto engine = GameEngine::create<PlayerDemoApplication>();
	auto engine = GameEngine::create<DummyApplication>();
	engine.spawn_object<CubeMap>(); // background/horizon
	Renderable floor_renderable;
	floor_renderable.pipeline_render_type = ERenderType::COLOR;
	floor_renderable.mesh_id = MeshFactory::cube_id();
	floor_renderable.material_ids = { MaterialFactory::fetch_preset(EMaterialPreset::DIFFUSE) };
	auto& floor = engine.spawn_object<Object>(floor_renderable);
	floor.set_scale(glm::vec3(100.0f, 0.1f, 100.0f));
	floor.set_position(glm::vec3(0.0f, -0.05f, 0.0f));
	engine.get_ecs().add_collider(floor.get_id(), std::make_unique<BoxCollider>());

	auto& obstacle = engine.spawn_object<Object>(Renderable::make_default(MeshFactory::cube_id()));
	obstacle.set_position({ 2.0f, 0.5f, 2.0f });
	obstacle.set_scale({ 1.0f, 1.0f, 1.0f });
	engine.get_ecs().add_collider(obstacle.get_id(), std::make_unique<BoxCollider>());

	auto& floating_obj1 = engine.spawn_object<Object>(Renderable::make_default(MeshFactory::cube_id()));
	floating_obj1.set_position(glm::vec3(0.0f, 1.5f, 0.0f));
	floating_obj1.set_rotation(glm::angleAxis(glm::radians(45.0f), glm::vec3(1.0f, 0.0f, 0.0f)));
	engine.get_ecs().add_clickable_entity(floating_obj1.get_id());
	engine.get_ecs().add_collider(floating_obj1.get_id(), std::make_unique<BoxCollider>());

	auto& light_source = engine.spawn_object<Object>(Renderable{
		.mesh_id = MeshFactory::sphere_id(),
		.material_ids = { MaterialFactory::fetch_preset(EMaterialPreset::LIGHT_SOURCE) },
		.pipeline_render_type = ERenderType::COLOR
	});
	light_source.set_position(glm::vec3(0.0f, 30.0f, 0.0f));
	LightComponent light_component{
		.intensity = 1.0f,
		.color = { 1.0f, 0.9f, 0.2f }
	};
	engine.get_ecs().add_light_source(light_source.get_id(), light_component);
	engine.get_ecs().add_clickable_entity(light_source.get_id());
	engine.get_ecs().add_collider(light_source.get_id(), std::make_unique<SphereCollider>());

	engine.run();
}
