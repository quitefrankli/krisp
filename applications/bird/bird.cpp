#include <camera.hpp>
#include <config.hpp>
#include <entity_component_system/light_source.hpp>
#include <entity_component_system/material_system.hpp>
#include <entity_component_system/mesh_system.hpp>
#include <entity_component_system/particle_system.hpp>
#include <game_engine.hpp>
#include <iapplication.hpp>
#include <maths.hpp>
#include <renderable/material.hpp>
#include <renderable/mesh.hpp>
#include <resource_loader/resource_loader.hpp>

#include <glm/gtc/quaternion.hpp>

#include <array>
#include <cmath>
#include <optional>
#include <utility>
#include <vector>


namespace
{
constexpr float JET_SCALE = 0.045f;
constexpr glm::vec3 JET_POSITION{ 0.0f, 1.3f, 3.5f };
constexpr float BANK_INTERVAL_SECONDS = 8.0f;
constexpr float BANK_START_SECONDS = 4.0f;
constexpr float BANK_DURATION_SECONDS = 2.5f;
constexpr float MAX_BANK_DEGREES = 22.0f;

class BirdApplication final : public DummyApplication
{
public:
	bool allows_playerless_normal_mode() const override { return true; }

	void on_begin(GameEngine& engine) override
	{
		base_orientation = Maths::RotationBetweenVectors(
			-Maths::right_vec,
			Maths::forward_vec);
		spawn_jet(engine);
		spawn_wing_trails(engine);
		spawn_particle_effects(engine);
		spawn_light(engine);

		engine.set_exposure_ev(0.35f);
		engine.get_camera().look_at(
			JET_POSITION + glm::vec3(0.0f, 0.5f, 1.8f),
			glm::vec3(0.0f, 5.2f, -8.5f));
		engine.set_normal_mode_cursor_captured(false);
		engine.set_game_mode(EGameMode::NORMAL);
	}

	void on_pre_tick(GameEngine& engine, const float delta_seconds) override
	{
		if (!jet_id)
			return;
		elapsed_seconds += delta_seconds;

		const float cycle = std::fmod(elapsed_seconds, BANK_INTERVAL_SECONDS);
		float bank = 0.0f;
		if (cycle >= BANK_START_SECONDS
			&& cycle < BANK_START_SECONDS + BANK_DURATION_SECONDS)
		{
			const float bank_phase = (cycle - BANK_START_SECONDS) / BANK_DURATION_SECONDS;
			const int cycle_index = static_cast<int>(elapsed_seconds / BANK_INTERVAL_SECONDS);
			const float direction = cycle_index % 2 == 0 ? 1.0f : -1.0f;
			bank = direction * Maths::deg2rad(MAX_BANK_DEGREES)
				* std::sin(Maths::PI * bank_phase);
		}

		const float pitch = Maths::deg2rad(1.5f) * std::sin(elapsed_seconds * 0.45f);
		const float yaw = Maths::deg2rad(1.0f) * std::sin(elapsed_seconds * 0.30f);
		const glm::quat orientation =
			glm::angleAxis(yaw, Maths::up_vec)
			* glm::angleAxis(pitch, Maths::right_vec)
			* glm::angleAxis(bank, Maths::forward_vec)
			* base_orientation;
		engine.get_ecs().set_rotation(*jet_id, orientation);
	}

private:
	void spawn_jet(GameEngine& engine)
	{
		ResourceLoader::LoadOptions options;
		options.regenerate_invalid_tangents = true;
		auto model = ResourceLoader::load_model(
			engine.get_ecs(),
			"fighter_jet.glb",
			options);
		std::vector<Renderable> renderables;
		for (auto& mesh : model.meshes)
		{
			// This asset includes both flight and landing configurations.
			if (mesh.source_node == 16 || mesh.source_node == 18)
				continue;
			for (auto& renderable : mesh.renderables)
				renderables.push_back(std::move(renderable));
		}

		auto& jet = engine.spawn_object<Object>();
		jet.set_name("MiG-29");
		engine.attach_renderables(jet.get_id(), std::move(renderables));
		jet_id = jet.get_id();

		auto& transform = engine.get_ecs().get_transformation(jet.get_id());
		transform.set_position(JET_POSITION);
		transform.set_scale(JET_SCALE);
		transform.set_rotation(base_orientation);
	}

	void spawn_particle_effects(GameEngine& engine) const
	{
		ParticleEmitterConfig exhaust;
		exhaust.max_particles = 400;
		exhaust.emission_rate = 240.0f;
		exhaust.min_lifetime = 0.16f;
		exhaust.max_lifetime = 0.38f;
		exhaust.min_size = 0.16f;
		exhaust.max_size = 0.48f;
		exhaust.start_color = { 1.0f, 0.85f, 0.35f, 0.95f };
		exhaust.end_color = { 1.0f, 0.12f, 0.01f, 0.0f };
		exhaust.velocity_min = { 5.0f, -0.55f, -0.55f };
		exhaust.velocity_max = { 8.0f, 0.55f, 0.55f };
		exhaust.emission_space = EParticleEmissionSpace::LOCAL;
		spawn_attached_emitter(engine, exhaust, { 48.0f, 0.0f, -10.0f });
		spawn_attached_emitter(engine, exhaust, { 48.0f, 0.0f, 10.0f });

		ParticleEmitterConfig atmosphere;
		atmosphere.max_particles = 450;
		atmosphere.emission_rate = 70.0f;
		atmosphere.min_lifetime = 3.0f;
		atmosphere.max_lifetime = 5.0f;
		atmosphere.min_size = 1.2f;
		atmosphere.max_size = 4.0f;
		atmosphere.start_color = { 0.95f, 0.98f, 1.0f, 0.10f };
		atmosphere.end_color = { 0.90f, 0.95f, 1.0f, 0.0f };
		atmosphere.spawn_offset_min = { -18.0f, -12.0f, 18.0f };
		atmosphere.spawn_offset_max = { 18.0f, 2.0f, 70.0f };
		atmosphere.velocity_min = { -0.5f, -0.1f, -18.0f };
		atmosphere.velocity_max = { 0.5f, 0.1f, -12.0f };
		engine.spawn_particle_emitter(atmosphere);
	}

	void spawn_wing_trails(GameEngine& engine) const
	{
		constexpr float length = 160.0f;
		constexpr float root_half_width = 0.30f;
		constexpr float tip_half_width = 0.025f;
		ColorVertices vertices{
			{{ 0.0f, 0.0f, -root_half_width }, Maths::up_vec},
			{{ 0.0f, 0.0f, root_half_width }, Maths::up_vec},
			{{ length, 0.0f, -tip_half_width }, Maths::up_vec},
			{{ length, 0.0f, tip_half_width }, Maths::up_vec},
			{{ 0.0f, -root_half_width, 0.0f }, Maths::forward_vec},
			{{ 0.0f, root_half_width, 0.0f }, Maths::forward_vec},
			{{ length, -tip_half_width, 0.0f }, Maths::forward_vec},
			{{ length, tip_half_width, 0.0f }, Maths::forward_vec},
		};
		VertexIndices indices{
			0, 2, 1, 1, 2, 3,
			4, 5, 6, 5, 7, 6,
		};
		const auto mesh = engine.get_ecs().get_mesh_system().add(
			std::make_unique<ColorMesh>(std::move(vertices), std::move(indices)));
		const auto material = engine.get_ecs().get_material_system().add(
			std::make_unique<PbrMaterial>(
				glm::vec4(0.84f, 0.94f, 1.0f, 0.42f),
				0.0f,
				1.0f,
				PbrMaterial::TextureSlots{},
				1.0f,
				PbrMaterial::Properties{
					.alpha_mode = EAlphaMode::BLEND,
					.double_sided = true,
					.emissive_factor = { 0.25f, 0.30f, 0.35f },
				}));

		for (const glm::vec3 position : std::array{
			glm::vec3{ 2.0f, -0.656f, -53.18f },
			glm::vec3{ 2.0f, -0.656f, 53.18f },
		})
		{
			Renderable trail{
				.name = "Wing trail",
				.pipeline_render_type = ERenderType::COLOR,
				.shading_mode = EShadingMode::UNLIT,
				.casts_shadow = false,
				.mesh_owner = mesh,
				.material_owners = { material },
			};
			auto& object = engine.spawn_object<Object>();
			object.set_name("Wing trail");
			engine.attach_renderable(object.get_id(), std::move(trail));
			auto& transform = engine.get_ecs().get_transformation(object.get_id());
			transform.attach_to(*jet_id);
			transform.set_relative_position(position);
			transform.set_relative_rotation(Maths::identity_quat);
			transform.set_relative_scale(glm::vec3(1.0f));
		}
	}

	void spawn_attached_emitter(
		GameEngine& engine,
		const ParticleEmitterConfig& config,
		const glm::vec3& relative_position) const
	{
		auto& emitter = engine.spawn_particle_emitter(config);
		auto& transform = engine.get_ecs().get_transformation(emitter.get_id());
		transform.attach_to(*jet_id);
		transform.set_relative_position(relative_position);
		transform.set_relative_rotation(Maths::identity_quat);
		transform.set_relative_scale(glm::vec3(1.0f));
	}

	static void spawn_light(GameEngine& engine)
	{
		auto& light = engine.spawn_object<Object>();
		light.set_name("Sunlight");
		engine.get_ecs().set_position(light.get_id(), { -12.0f, 20.0f, -6.0f });
		engine.get_ecs().add_light_source(light.get_id(), LightComponent{
			.intensity = 3000.0f,
			.color = { 1.0f, 0.95f, 0.90f },
		});
	}

	std::optional<ObjectID> jet_id;
	glm::quat base_orientation = Maths::identity_quat;
	float elapsed_seconds = 0.0f;
};
}

int main()
{
	Config::init(PROJECT_NAME);
	auto engine = GameEngine::create<BirdApplication>();
	engine.spawn_cubemap(PROJECT_ENVIRONMENT_LIGHTING_ASSET);
	engine.run();
}
