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

#include "krisp_ui.hpp"

#include <fmt/core.h>
#include <fmt/color.h>

#include <iostream>
#include <thread>
#include <iomanip>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <utility>

#include <glm/gtc/matrix_transform.hpp>


namespace
{
constexpr std::string_view player_model = "npc.glb";
constexpr std::string_view player_animations = "movement_animations.glb";
constexpr std::string_view attack_animation_file = "combat_animations.glb";
constexpr std::string_view attack_animation_name = "1h_sword_attack";

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
	void create_ui(GameEngine&, ApplicationUiManager& ui) override
	{
		ui.set_theme({
			.text = { 0.96f, 0.91f, 0.78f, 1.0f },
			.window_background = { 0.09f, 0.10f, 0.11f, 0.90f },
			.accent = { 0.88f, 0.52f, 0.12f, 1.0f },
			.window_rounding = 9.0f,
			.window_border_size = 1.0f,
		});
		ui.register_overlay<KrispStatusOverlay>({
			.anchor = ApplicationUiAnchor::TOP_LEFT,
			.offset = { 16.0f, 16.0f },
			.size = { 250.0f, 70.0f },
		}, ui_state);
		ui.register_window<KrispEquipmentWindow>({
			.anchor = ApplicationUiAnchor::TOP_RIGHT,
			.offset = { -16.0f, 16.0f },
			.size = { 280.0f, 150.0f },
		}, ui_state);
	}

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
		const auto imported_attack =
			ResourceLoader::load_animations(engine.get_ecs(), attack_animation_file, skeleton);
		attack_animation =
			require_animation(engine.get_ecs(), imported_attack, attack_animation_name);
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
		for (auto& renderable : mesh->renderables)
		{
			const glm::mat4 face_gameplay_forward =
				glm::rotate(Maths::identity_mat, Maths::PI, Maths::up_vec);
			renderable.local_transform.set_mat4(
				face_gameplay_forward * renderable.local_transform.get_mat4());
		}
		auto& spawned_player = engine.spawn_object<PlayerCharacter>(
			std::move(mesh->renderables), definition);
		spawned_player.configure_locomotion(skeleton, locomotion);
		engine.get_ecs().attach_skeleton(spawned_player.get_id(), skeleton);
		engine.get_ecs().add_collider(spawned_player.get_id(), std::make_unique<CapsuleCollider>(
			definition.capsule_radius, definition.capsule_height));
		engine.get_ecs().add_clickable_entity(spawned_player.get_id());
		spawned_player.set_name("Player");

		auto& temporary_sword = engine.spawn_object<Object>(
			Renderable::make_default(MeshSystem::add(MeshFactory::cylinder())));
		temporary_sword.set_name("Temporary Sword");
		Maths::Transform sword_grip;
		sword_grip.set_scale({ 0.06f, 0.7f, 0.06f });
		sword_definition.grip_transform = sword_grip;
		if (!engine.get_ecs().equip(spawned_player.get_id(), temporary_sword.get_id(), sword_definition))
			throw std::runtime_error("Player skeleton is missing the WEAPON bone");
		player_id = spawned_player.get_id();
		temporary_sword_id = temporary_sword.get_id();
		ui_state.publish(spawned_player.is_moving(), true);

		auto& camera = engine.get_camera();
		camera.look_at(spawned_player.get_position() + definition.camera_focus_offset,
			spawned_player.get_position() + glm::vec3(0.0f, 2.0f, -5.0f));
		engine.set_game_mode(EGameMode::EDITOR);
		// engine.set_camera_orbit_with_right_mouse(true);
	}

	void on_tick(GameEngine& engine, float) override
	{
		if (!player_id || !temporary_sword_id)
			return;

		auto& ecs = engine.get_ecs();
		if (ui_state.take_main_hand_toggle_request())
		{
			if (ecs.equipped_item(*player_id, EquipmentSlot::MainHand))
				ecs.unequip(*player_id, EquipmentSlot::MainHand);
			else if (!ecs.equip(*player_id, *temporary_sword_id, sword_definition))
				throw std::runtime_error("Player skeleton is missing the WEAPON bone");
		}

		const auto* player = engine.get_active_player();
		ui_state.publish(player && player->is_moving(),
			static_cast<bool>(ecs.equipped_item(*player_id, EquipmentSlot::MainHand)));
	}

	void on_click(GameEngine&, Object&) override {}
	void on_mouse_button(GameEngine& engine, const MouseInput& input) override
	{
		if (!input.eq(EMouseButton::LEFT, EKeyModifier::NONE, EInputAction::PRESS)
			|| !attack_animation)
			return;
		auto* player = engine.get_active_player();
		if (player && !player->play_action_animation(engine.get_ecs(), *attack_animation))
			throw std::runtime_error("Attack animation is incompatible with the player skeleton");
	}
	void on_key_press(GameEngine&, const KeyInput&) override {}

private:
	KrispUiState ui_state;
	std::optional<EntityID> player_id;
	std::optional<EntityID> temporary_sword_id;
	std::optional<AnimationID> attack_animation;
	EquipmentDefinition sword_definition{
		.slot = EquipmentSlot::MainHand,
		.attachment_bone = "WEAPON",
	};
};
}


int main(int argc, char* argv[])
{
	Config::init(PROJECT_NAME);
	auto engine = GameEngine::create<PlayerDemoApplication>();
	engine.spawn_cubemap(); // background/horizon
	Renderable floor_renderable;
	floor_renderable.pipeline_render_type = ERenderType::COLOR;
	floor_renderable.mesh_owner = MeshSystem::add(MeshFactory::cube());
	floor_renderable.material_owners = {
		MaterialSystem::add(MaterialFactory::fetch_preset(EMaterialPreset::DIFFUSE))
	};
	auto& floor = engine.spawn_object<Object>(std::move(floor_renderable));
	floor.set_scale(glm::vec3(100.0f, 0.1f, 100.0f));
	floor.set_position(glm::vec3(0.0f, -0.05f, 0.0f));
	engine.get_ecs().add_collider(floor.get_id(), std::make_unique<BoxCollider>());

	auto& obstacle = engine.spawn_object<Object>(
		Renderable::make_default(MeshSystem::add(MeshFactory::cube())));
	obstacle.set_position({ 2.0f, 0.5f, 2.0f });
	obstacle.set_scale({ 1.0f, 1.0f, 1.0f });
	engine.get_ecs().add_collider(obstacle.get_id(), std::make_unique<BoxCollider>());
	engine.get_ecs().add_clickable_entity(obstacle.get_id());

	Renderable light_renderable;
	light_renderable.mesh_owner = MeshSystem::add(MeshFactory::sphere());
	light_renderable.material_owners = {
		MaterialSystem::add(MaterialFactory::fetch_preset(EMaterialPreset::LIGHT_SOURCE))
	};
	light_renderable.pipeline_render_type = ERenderType::COLOR;
	auto& light_source = engine.spawn_object<Object>(std::move(light_renderable));
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
