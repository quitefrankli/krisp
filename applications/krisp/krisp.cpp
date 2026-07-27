#include <game_engine.hpp>
#include <camera.hpp>
#include <graphics_engine/graphics_engine.hpp>
#include <iapplication.hpp>
#include <window.hpp>
#include <config.hpp>
#include <utility.hpp>
#include <renderable/mesh_factory.hpp>
#include <renderable/material_factory.hpp>
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
#include <string>

#include <glm/gtc/matrix_transform.hpp>


namespace
{
constexpr std::string_view player_model = "npc.glb";
constexpr std::string_view player_animations = "movement_animations.glb";

AnimationID require_animation(
	const ECS& ecs,
	const ResourceLoader::LoadedAnimations& imported,
	const std::string_view name)
{
	const auto& animations = ecs.get_skeletal_animations();
	const auto found = std::ranges::find_if(imported.animations, [&](const AnimationID id)
	{
		return animations.at(id).name == name;
	});
	if (found == imported.animations.end())
		throw std::runtime_error("Missing required player animation: " + std::string(name));
	return *found;
}

class PlayerDemoApplication : public IApplication
{
public:
	void on_begin(GameEngine& engine) override
	{
		auto model = ResourceLoader::load_model(engine.get_ecs(), player_model);
		auto mesh = std::ranges::find_if(model.meshes, [](const auto& candidate)
		{
			return candidate.skeleton_id.has_value();
		});
		if (mesh == model.meshes.end())
			throw std::runtime_error("Player model must contain a skinned mesh");
		const SkeletonID skeleton = *mesh->skeleton_id;
		const auto imported_animations =
			ResourceLoader::load_animations(engine.get_ecs(), player_animations, skeleton);
		const PlayerLocomotionAnimations locomotion{
			.idle = require_animation(engine.get_ecs(), imported_animations, "idle"),
			.walk_backward = require_animation(engine.get_ecs(), imported_animations, "walkbackward"),
			.walk_backward_left = require_animation(engine.get_ecs(), imported_animations, "walkbackwardleft"),
			.walk_backward_right = require_animation(engine.get_ecs(), imported_animations, "walkbackwardright"),
			.walk_forward = require_animation(engine.get_ecs(), imported_animations, "walkforward"),
			.walk_forward_left = require_animation(engine.get_ecs(), imported_animations, "walkforwardleft"),
			.walk_forward_right = require_animation(engine.get_ecs(), imported_animations, "walkforwardright"),
			.walk_left = require_animation(engine.get_ecs(), imported_animations, "walkleft"),
			.walk_right = require_animation(engine.get_ecs(), imported_animations, "walkright"),
		};

		PlayerDefinition definition;
		auto& spawned_player = engine.spawn_object<PlayerCharacter>(
			mesh->renderables, definition);
		spawned_player.configure_locomotion(skeleton, locomotion);
		engine.get_ecs().attach_skeleton(spawned_player.get_id(), skeleton);
		engine.get_ecs().add_collider(spawned_player.get_id(), std::make_unique<CapsuleCollider>(
			definition.capsule_radius, definition.capsule_height));
		engine.get_ecs().add_clickable_entity(spawned_player.get_id());
		spawned_player.set_name("Player");

		auto& camera = engine.get_camera();
		camera.look_at(spawned_player.get_position() + definition.camera_focus_offset,
			spawned_player.get_position() + glm::vec3(0.0f, 2.0f, -5.0f));
		// engine.set_game_mode(EGameMode::NORMAL);
		// engine.set_camera_orbit_with_right_mouse(true);
	}

	void on_tick(GameEngine&, float) override {}

	void on_click(GameEngine&, Object&) override {}
	void on_key_press(GameEngine&, const KeyInput&) override {}
};
}


int main(int argc, char* argv[])
{
	Config::init(PROJECT_NAME);
	auto engine = GameEngine::create<PlayerDemoApplication>();
	engine.spawn_cubemap(); // background/horizon
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
	engine.get_ecs().add_clickable_entity(obstacle.get_id());

	auto& light_source = engine.spawn_object<Object>(Renderable{
		.mesh_id = MeshFactory::sphere_id(),
		.material_ids = { MaterialFactory::fetch_preset(EMaterialPreset::LIGHT_SOURCE) },
		.pipeline_render_type = ERenderType::COLOR
	});
	light_source.set_position(glm::vec3(0.5f, 6.0f, -3.0f));
	LightComponent light_component{
		.intensity = 1.0f,
		.color = { 1.0f, 0.9f, 0.2f }
	};
	engine.get_ecs().add_light_source(light_source.get_id(), light_component);
	engine.get_ecs().add_collider(light_source.get_id(), std::make_unique<SphereCollider>());
	engine.get_ecs().add_clickable_entity(light_source.get_id());

	engine.run();
}
