#include "rpg_ui.hpp"

#include <camera.hpp>
#include <config.hpp>
#include <entity_component_system/light_source.hpp>
#include <entity_component_system/physics/physics.hpp>
#include <game_engine.hpp>
#include <game_objects/player_character.hpp>
#include <iapplication.hpp>
#include <maths.hpp>
#include <objects/object.hpp>
#include <renderable/material.hpp>
#include <renderable/mesh_factory.hpp>
#include <resource_loader/resource_loader.hpp>

#include <glm/gtc/matrix_transform.hpp>

#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>


namespace
{
constexpr std::string_view player_model = "npc.glb";
constexpr std::string_view player_animations = "movement_animations.glb";
constexpr std::string_view sword_model = "weapons_iron_longsword.glb";

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

class RpgApplication final : public IApplication
{
public:
	void create_ui(GameEngine&, ApplicationUiManager& ui) override
	{
		ui.register_overlay<RpgStatusOverlay>({
			.anchor = ApplicationUiAnchor::TOP_LEFT,
			.offset = { 16.0f, 16.0f },
			.size = { 250.0f, 70.0f },
		}, ui_state);
		ui.register_window<RpgEquipmentWindow>({
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
		const auto imported = ResourceLoader::load_animations(
			engine.get_ecs(), player_animations, skeleton);
		const PlayerLocomotionAnimations locomotion{
			.idle = require_animation(engine.get_ecs(), imported, "idle"),
			.walk_backward = require_animation(engine.get_ecs(), imported, "walkbackward"),
			.walk_backward_left = require_animation(engine.get_ecs(), imported, "walkbackwardright"),
			.walk_backward_right = require_animation(engine.get_ecs(), imported, "walkbackwardleft"),
			.walk_forward = require_animation(engine.get_ecs(), imported, "walkforward"),
			.walk_forward_left = require_animation(engine.get_ecs(), imported, "walkforwardright"),
			.walk_forward_right = require_animation(engine.get_ecs(), imported, "walkforwardleft"),
			.walk_left = require_animation(engine.get_ecs(), imported, "walkright"),
			.walk_right = require_animation(engine.get_ecs(), imported, "walkleft"),
		};

		PlayerDefinition definition;
		for (auto& renderable : mesh->renderables)
		{
			const glm::mat4 face_gameplay_forward =
				glm::rotate(Maths::identity_mat, Maths::PI, Maths::up_vec);
			renderable.local_transform.set_mat4(
				face_gameplay_forward * renderable.local_transform.get_mat4());
		}
		auto& player = engine.spawn_object<PlayerCharacter>(definition);
		const auto renderables = engine.attach_renderables(
			player.get_id(), std::move(mesh->renderables), skeleton);
		if (renderables.empty())
			throw std::runtime_error("Player model must contain a renderable");
		player.configure_locomotion(skeleton, locomotion);
		player.set_name("Player");
		engine.get_ecs().add_rigid_body(player.get_id(), RigidBodyDefinition{
			.shape = CapsulePhysicsShape{ definition.capsule_radius, definition.capsule_height },
			.motion = PhysicsMotionType::Kinematic,
		});

		auto loaded_sword = ResourceLoader::load_model(engine.get_ecs(), sword_model);
		if (loaded_sword.meshes.empty())
			throw std::runtime_error("Iron Longsword model contains no meshes");
		auto& sword = engine.spawn_object<Object>();
		engine.attach_renderables(
			sword.get_id(), std::move(loaded_sword.meshes.front().renderables));
		sword.set_name("Iron Longsword");
		if (!engine.get_ecs().equip(
			player.get_id(), renderables.front(), sword.get_id(), sword_definition))
			throw std::runtime_error("Player skeleton is missing the WEAPON bone");

		player_id = player.get_id();
		sword_id = sword.get_id();
		player_renderable = renderables.front();
		ui_state.publish(false, true);
		engine.get_camera().look_at(
			definition.camera_focus_offset,
			glm::vec3(0.0f, 2.0f, -5.0f));
		engine.set_game_mode(EGameMode::NORMAL);
	}

	void on_tick(GameEngine& engine, float) override
	{
		if (!player_id || !sword_id || !player_renderable)
			return;
		auto& ecs = engine.get_ecs();
		if (ui_state.take_main_hand_toggle_request())
		{
			if (ecs.equipped_item(*player_id, EquipmentSlot::MainHand))
				ecs.unequip(*player_id, EquipmentSlot::MainHand);
			else if (!ecs.equip(*player_id, *player_renderable, *sword_id, sword_definition))
				throw std::runtime_error("Player skeleton is missing the WEAPON bone");
		}
		const auto* player = engine.get_active_player();
		ui_state.publish(player && player->is_moving(),
			static_cast<bool>(ecs.equipped_item(*player_id, EquipmentSlot::MainHand)));
	}

	void on_click(GameEngine&, Object&) override {}
	void on_key_press(GameEngine&, const KeyInput&) override {}

private:
	RpgUiState ui_state;
	std::optional<EntityID> player_id;
	std::optional<EntityID> sword_id;
	std::optional<RenderableID> player_renderable;
	EquipmentDefinition sword_definition{
		.slot = EquipmentSlot::MainHand,
		.attachment_bone = "WEAPON",
	};
};

void spawn_floor(GameEngine& engine)
{
	Renderable floor_renderable{
		.pipeline_render_type = ERenderType::COLOR,
		.mesh_owner = engine.get_ecs().get_mesh_system().add(MeshFactory::cube()),
		.material_owners = {
			engine.get_ecs().get_material_system().add(
				std::make_unique<PbrMaterial>(
					glm::vec4(0.3f, 0.35f, 0.3f, 1.0f), 0.0f, 0.9f)),
		},
	};
	auto& floor = engine.spawn_object<Object>();
	engine.attach_renderable(floor.get_id(), std::move(floor_renderable));
	auto& transform = engine.get_ecs().get_transformation(floor.get_id());
	transform.set_scale(glm::vec3(100.0f, 0.1f, 100.0f));
	transform.set_position(glm::vec3(0.0f, -0.05f, 0.0f));
	engine.get_ecs().add_rigid_body(floor.get_id(), RigidBodyDefinition{
		.shape = BoxPhysicsShape{ { 50.0f, 0.05f, 50.0f } },
	});
}

void spawn_point_light(GameEngine& engine)
{
	auto& light = engine.spawn_object<Object>();
	light.set_name("RPG point light");
	engine.get_ecs().get_transformation(light.get_id())
		.set_position(glm::vec3(0.0f, 6.5f, -7.0f));
	engine.get_ecs().add_light_source(light.get_id(), LightComponent{
		.intensity = 225.0f,
		.color = glm::vec3(1.0f),
	});

	Renderable marker{
		.name = "Point-light handle",
		.pipeline_render_type = ERenderType::COLOR,
		.shading_mode = EShadingMode::UNLIT,
		.casts_shadow = false,
		.mesh_owner = engine.get_ecs().get_mesh_system().add(MeshFactory::sphere(
			MeshFactory::EVertexType::COLOR,
			MeshFactory::GenerationMethod::ICO_SPHERE,
			100)),
		.material_owners = {
			engine.get_ecs().get_material_system().add(
				std::make_unique<PbrMaterial>(
					glm::vec4(1.0f, 0.55f, 0.05f, 1.0f), 0.0f, 0.25f)),
		},
	};
	marker.local_transform.set_scale(glm::vec3(1.0f / 3.0f));
	engine.attach_renderable(light.get_id(), std::move(marker));
	engine.get_ecs().add_rigid_body(light.get_id(), RigidBodyDefinition{
		.shape = SpherePhysicsShape{ 1.2f },
		.participation = PhysicsParticipation::QueryOnly,
	});
	engine.get_ecs().add_clickable_entity(light.get_id());
}

void spawn_landmark(GameEngine& engine)
{
	Renderable renderable{
		.name = "Landmark cube",
		.pipeline_render_type = ERenderType::COLOR,
		.mesh_owner = engine.get_ecs().get_mesh_system().add(MeshFactory::cube()),
		.material_owners = {
			engine.get_ecs().get_material_system().add(
				std::make_unique<PbrMaterial>(
					glm::vec4(0.1f, 0.35f, 0.85f, 1.0f), 0.1f, 0.35f)),
		},
	};
	auto& landmark = engine.spawn_object<Object>();
	landmark.set_name("Landmark cube");
	engine.attach_renderable(landmark.get_id(), std::move(renderable));
	auto& transform = engine.get_ecs().get_transformation(landmark.get_id());
	transform.set_position(glm::vec3(3.0f, 0.5f, 0.0f));
	engine.get_ecs().add_rigid_body(landmark.get_id(), RigidBodyDefinition{});
	engine.get_ecs().add_clickable_entity(landmark.get_id());
}
}

int main()
{
	Config::init(PROJECT_NAME);
	auto engine = GameEngine::create<RpgApplication>();
	engine.spawn_cubemap(PROJECT_ENVIRONMENT_LIGHTING_ASSET);
	spawn_floor(engine);
	spawn_point_light(engine);
	spawn_landmark(engine);
	engine.run();
}
