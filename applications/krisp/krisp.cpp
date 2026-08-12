#include <camera.hpp>
#include <config.hpp>
#include <entity_component_system/light_source.hpp>
#include <entity_component_system/physics/physics.hpp>
#include <game_engine.hpp>
#include <iapplication.hpp>
#include <objects/object.hpp>
#include <renderable/material.hpp>
#include <renderable/mesh_factory.hpp>

#include <array>
#include <cstddef>
#include <memory>
#include <utility>


namespace
{
class PbrProofApplication final : public DummyApplication
{
public:
	void on_begin(GameEngine& engine) override
	{
		engine.set_exposure_ev(0.0f);
		engine.get_camera().look_at(
			glm::vec3(0.0f, 2.4f, 0.8f),
			glm::vec3(0.0f, 5.5f, -15.0f));

		spawn_factor_grid(engine);
		spawn_floor(engine);
		spawn_point_light(engine);
	}

private:
	static void spawn_floor(GameEngine& engine)
	{
		Renderable renderable{
			.name = "PBR shadow receiver",
			.pipeline_render_type = ERenderType::COLOR,
			.mesh_owner = engine.get_ecs().get_mesh_system().add(
				MeshFactory::cube(MeshFactory::EVertexType::COLOR)),
			.material_owners = {
				engine.get_ecs().get_material_system().add(
					std::make_unique<PbrMaterial>(
						glm::vec4(0.45f, 0.45f, 0.45f, 1.0f), 0.0f, 0.9f)),
			},
		};
		auto& floor = engine.spawn_object<Object>();
		floor.set_name("PBR shadow receiver");
		engine.attach_renderable(floor.get_id(), std::move(renderable));
		auto& transform = engine.get_ecs().get_transformation(floor.get_id());
		transform.set_position(glm::vec3(0.0f, -0.2f, 0.5f));
		transform.set_scale(glm::vec3(8.5f, 0.1f, 7.0f));
	}

	static void spawn_point_light(GameEngine& engine)
	{
		auto& light = engine.spawn_object<Object>();
		light.set_name("PBR reference point light");
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
			.shape = SpherePhysicsShape{1.2f},
			.participation = PhysicsParticipation::QueryOnly,
		});
		engine.get_ecs().add_clickable_entity(light.get_id());
	}

	static void spawn_factor_grid(GameEngine& engine)
	{
		const auto sphere = engine.get_ecs().get_mesh_system().add(MeshFactory::sphere(
			MeshFactory::EVertexType::COLOR,
			MeshFactory::GenerationMethod::ICO_SPHERE,
			400));
		constexpr std::array roughness{ 0.04f, 0.25f, 0.5f, 0.75f, 1.0f };
		constexpr std::array metallic{ 0.0f, 0.5f, 1.0f };
		constexpr glm::vec4 base_color(0.8f, 0.15f, 0.05f, 1.0f);

		for (size_t row = 0; row < metallic.size(); ++row)
		{
			for (size_t column = 0; column < roughness.size(); ++column)
			{
				Renderable renderable{
					.name = "PBR factor reference",
					.pipeline_render_type = ERenderType::COLOR,
					.mesh_owner = sphere,
					.material_owners = {
						engine.get_ecs().get_material_system().add(
							std::make_unique<PbrMaterial>(
								base_color, metallic[row], roughness[column])),
					},
				};
				auto& object = engine.spawn_object<Object>();
				object.set_name("PBR factor reference");
				engine.attach_renderable(object.get_id(), std::move(renderable));

				auto& transform = engine.get_ecs().get_transformation(object.get_id());
				transform.set_position(glm::vec3(
					(static_cast<float>(column) - 2.0f) * 1.7f,
					0.8f + static_cast<float>(row) * 1.7f,
					0.0f));
				transform.set_scale(glm::vec3(0.85f));
			}
		}
	}
};
}

int main()
{
	Config::init(PROJECT_NAME);
	auto engine = GameEngine::create<PbrProofApplication>();
	engine.spawn_cubemap(PROJECT_ENVIRONMENT_LIGHTING_ASSET);
	engine.run();
}
