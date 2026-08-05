#include <camera.hpp>
#include <config.hpp>
#include <entity_component_system/light_source.hpp>
#include <entity_component_system/physics/physics.hpp>
#include <entity_component_system/skeletal.hpp>
#include <game_engine.hpp>
#include <iapplication.hpp>
#include <objects/object.hpp>
#include <renderable/material.hpp>
#include <renderable/mesh_factory.hpp>

#include <array>
#include <string>


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
		spawn_floor_and_transform_reference(engine);
		spawn_skinned_reference(engine);
		spawn_point_light(engine);
	}

private:
	static void attach_pbr_mesh(
		GameEngine& engine,
		const MeshHandle& mesh,
		const std::string& name,
		const glm::vec3 position,
		const glm::vec3 scale,
		const glm::vec4 base_color,
		const float metallic,
		const float roughness)
	{
		Renderable renderable{
			.name = name,
			.pipeline_render_type = ERenderType::COLOR,
			.mesh_owner = mesh,
			.material_owners = {
				engine.get_ecs().get_material_system().add(
					std::make_unique<PbrMaterial>(base_color, metallic, roughness)),
			},
		};
		auto& object = engine.spawn_object<Object>();
		object.set_name(name);
		engine.attach_renderable(object.get_id(), std::move(renderable));
		auto& transform = engine.get_ecs().get_transformation(object.get_id());
		transform.set_position(position);
		transform.set_scale(scale);
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
				attach_pbr_mesh(
					engine,
					sphere,
					"PBR factor reference",
					glm::vec3(
						(static_cast<float>(column) - 2.0f) * 1.7f,
						0.8f + static_cast<float>(row) * 1.7f,
						0.0f),
					glm::vec3(0.85f),
					base_color,
					metallic[row],
					roughness[column]);
			}
		}
	}

	static void spawn_floor_and_transform_reference(GameEngine& engine)
	{
		const auto cube = engine.get_ecs().get_mesh_system().add(
			MeshFactory::cube(MeshFactory::EVertexType::COLOR));
		attach_pbr_mesh(
			engine,
			cube,
			"PBR shadow receiver",
			glm::vec3(0.0f, -0.2f, 0.5f),
			glm::vec3(8.5f, 0.1f, 7.0f),
			glm::vec4(0.45f, 0.45f, 0.45f, 1.0f),
			0.0f,
			0.9f);
		attach_pbr_mesh(
			engine,
			cube,
			"Non-uniform normal reference and occluder",
			glm::vec3(4.8f, 0.8f, -0.2f),
			glm::vec3(0.6f, 1.2f, 0.4f),
			glm::vec4(0.15f, 0.7f, 0.2f, 1.0f),
			0.0f,
			0.35f);
	}

	static void spawn_skinned_reference(GameEngine& engine)
	{
		auto capsule = MeshFactory::capsule(0.45f, 2.4f, 32, 8);
		const auto& source = dynamic_cast<const ColorMesh&>(*capsule);
		SkinnedVertices vertices;
		vertices.reserve(source.get_vertices().size());
		for (const auto& source_vertex : source.get_vertices())
		{
			SDS::SkinnedVertex vertex{};
			vertex.pos = source_vertex.pos;
			vertex.normal = source_vertex.normal;
			vertex.texCoord = glm::vec2(0.0f);
			vertex.tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
			vertex.bone_ids = glm::vec4(0.0f);
			vertex.bone_weights = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
			vertices.push_back(vertex);
		}
		const auto mesh = engine.get_ecs().get_mesh_system().add(
			std::make_unique<SkinnedMesh>(
				std::move(vertices), VertexIndices(source.get_indices())));
		Bone root;
		root.name = "PBR proof root";
		const SkeletonID skeleton = engine.get_ecs().add_skeleton({ root });
		Renderable renderable{
			.name = "Skinned PBR capsule",
			.pipeline_render_type = ERenderType::SKINNED_COLOR,
			.mesh_owner = mesh,
			.material_owners = {
				engine.get_ecs().get_material_system().add(
					std::make_unique<PbrMaterial>(
						glm::vec4(0.08f, 0.35f, 0.85f, 1.0f), 0.25f, 0.35f)),
			},
		};
		auto& object = engine.spawn_object<Object>();
		object.set_name("Skinned PBR capsule");
		engine.attach_renderable(object.get_id(), std::move(renderable), skeleton);
		auto& transform = engine.get_ecs().get_transformation(object.get_id());
		transform.set_position(glm::vec3(4.8f, 2.7f, 0.0f));
		transform.set_scale(glm::vec3(0.75f));
	}

	static void spawn_point_light(GameEngine& engine)
	{
		auto& light = engine.spawn_object<Object>();
		light.set_name("PBR reference point light");
		engine.get_ecs().get_transformation(light.get_id())
			.set_position(glm::vec3(0.0f, 6.5f, -7.0f));

		Renderable marker{
			.name = "Point-light handle",
			.pipeline_render_type = ERenderType::COLOR,
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
		// Keep the visible handle just below the mathematical point so the light
		// illuminates it instead of sitting inside its own geometry. The gizmo
		// remains centred on the actual light position.
		marker.local_transform.set_pos(glm::vec3(0.0f, -0.65f, 0.0f));
		engine.attach_renderable(light.get_id(), std::move(marker));
		engine.get_ecs().add_light_source(light.get_id(), LightComponent{
			.intensity = 225.0f,
			.color = glm::vec3(1.0f),
		});
		engine.get_ecs().add_rigid_body(light.get_id(), RigidBodyDefinition{
			.shape = SpherePhysicsShape{1.2f},
			.participation = PhysicsParticipation::QueryOnly,
		});
		engine.get_ecs().add_clickable_entity(light.get_id());
	}
};
}

int main()
{
	Config::init(PROJECT_NAME);
	auto engine = GameEngine::create<PbrProofApplication>();
	engine.spawn_cubemap();
	engine.run();
}
