#include <camera.hpp>
#include <config.hpp>
#include <entity_component_system/light_source.hpp>
#include <entity_component_system/physics/physics.hpp>
#include <entity_component_system/skeletal.hpp>
#include <game_engine.hpp>
#include <iapplication.hpp>
#include <maths.hpp>
#include <objects/object.hpp>
#include <renderable/material.hpp>
#include <renderable/mesh_factory.hpp>
#include <resource_loader/resource_loader.hpp>

#include <array>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <vector>


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
		spawn_texture_proof(engine);
		spawn_stage3_material_proof(engine);
		spawn_point_light(engine);
	}

private:
	struct ProofTextures
	{
		MaterialHandle base_color;
		MaterialHandle metallic_roughness;
		MaterialHandle normal;
	};

	struct TextureProofCase
	{
		const char* name;
		glm::vec4 base_color_factor;
		float metallic_factor;
		float roughness_factor;
		PbrMaterial::TextureSlots textures;
		float normal_scale = 1.0f;
		bool repeated_uvs = false;
		bool non_uniform_transform = false;
	};

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

	static MeshHandle make_texture_cube(
		GameEngine& engine, const float uv_scale, const bool rotate_vertices = false)
	{
		auto source_mesh = MeshFactory::cube(MeshFactory::EVertexType::TEXTURE);
		const auto& source = dynamic_cast<const TexMesh&>(*source_mesh);
		auto vertices = source.get_vertices();
		const glm::mat3 vertex_rotation = glm::mat3_cast(glm::angleAxis(
			Maths::PI * 0.2f, glm::normalize(glm::vec3(1.0f, 1.0f, 0.5f))));
		for (auto& vertex : vertices)
		{
			vertex.texCoord *= uv_scale;
			if (rotate_vertices)
			{
				vertex.pos = vertex_rotation * vertex.pos;
				vertex.normal = vertex_rotation * vertex.normal;
				vertex.tangent = glm::vec4(
					vertex_rotation * glm::vec3(vertex.tangent), vertex.tangent.w);
			}
		}
		return engine.get_ecs().get_mesh_system().add(std::make_unique<TexMesh>(
			std::move(vertices), VertexIndices(source.get_indices())));
	}

	static MeshHandle make_skinned_texture_cube(
		GameEngine& engine, const MeshHandle& texture_cube)
	{
		const auto& source = dynamic_cast<const TexMesh&>(texture_cube->get());
		SkinnedVertices vertices;
		vertices.reserve(source.get_vertices().size());
		for (const auto& source_vertex : source.get_vertices())
		{
			SDS::SkinnedVertex vertex{};
			vertex.pos = source_vertex.pos;
			vertex.normal = source_vertex.normal;
			vertex.texCoord = source_vertex.texCoord;
			vertex.tangent = source_vertex.tangent;
			vertex.bone_ids = glm::vec4(0.0f);
			vertex.bone_weights = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
			vertices.push_back(vertex);
		}
		return engine.get_ecs().get_mesh_system().add(std::make_unique<SkinnedMesh>(
			std::move(vertices), VertexIndices(source.get_indices())));
	}

	static void attach_texture_proof_mesh(
		GameEngine& engine,
		const MeshHandle& mesh,
		const std::optional<SkeletonID> skeleton,
		const ProofTextures& owners,
		const TextureProofCase& proof,
		const glm::vec3 position)
	{
		auto material = engine.get_ecs().get_material_system().add(
			std::make_unique<PbrMaterial>(
				proof.base_color_factor,
				proof.metallic_factor,
				proof.roughness_factor,
				proof.textures,
				proof.normal_scale));
		std::vector<MaterialHandle> material_owners{ std::move(material) };
		const auto retain = [&](
			const std::optional<PbrMaterial::TextureBinding>& binding,
			const MaterialHandle& owner)
		{
			if (binding)
				material_owners.push_back(owner);
		};
		retain(proof.textures.base_color, owners.base_color);
		retain(proof.textures.metallic_roughness, owners.metallic_roughness);
		retain(proof.textures.normal, owners.normal);

		const std::string name = std::string(skeleton ? "Skinned " : "Static ") + proof.name;
		Renderable renderable{
			.name = name,
			.pipeline_render_type = skeleton ? ERenderType::SKINNED : ERenderType::STANDARD,
			.mesh_owner = mesh,
			.material_owners = std::move(material_owners),
		};
		auto& object = engine.spawn_object<Object>();
		object.set_name(name);
		engine.attach_renderable(object.get_id(), std::move(renderable), skeleton);
		auto& transform = engine.get_ecs().get_transformation(object.get_id());
		transform.set_position(position);
		if (proof.non_uniform_transform)
			transform.set_scale(glm::vec3(-0.38f, 0.68f, 0.28f));
		else
			transform.set_scale(glm::vec3(skeleton ? 0.48f : 0.55f));
	}

	static void spawn_texture_proof(GameEngine& engine)
	{
		auto& materials = engine.get_ecs().get_material_system();
		const ProofTextures texture_owners{
			.base_color = ResourceLoader::fetch_texture(
				materials, "pbr_proof_base.png", ETextureSemantic::BASE_COLOR),
			.metallic_roughness = ResourceLoader::fetch_texture(
				materials,
				"pbr_proof_metallic_roughness.png",
				ETextureSemantic::METALLIC_ROUGHNESS),
			.normal = ResourceLoader::fetch_texture(
				materials, "pbr_proof_normal.png", ETextureSemantic::NORMAL),
		};
		const auto binding = [](
			const MaterialHandle& owner,
			const PbrMaterial::TextureSampler sampler = PbrMaterial::TextureSampler::repeat())
		{
			return PbrMaterial::TextureBinding{ owner->get_id(), sampler };
		};

		PbrMaterial::TextureSlots base_only;
		base_only.base_color = binding(texture_owners.base_color);
		PbrMaterial::TextureSlots metallic_roughness_only;
		metallic_roughness_only.metallic_roughness = binding(
			texture_owners.metallic_roughness);
		PbrMaterial::TextureSlots normal_only;
		normal_only.normal = binding(texture_owners.normal);
		PbrMaterial::TextureSlots combined{
			.base_color = binding(texture_owners.base_color),
			.metallic_roughness = binding(texture_owners.metallic_roughness),
			.normal = binding(texture_owners.normal),
		};
		PbrMaterial::TextureSlots clamped_base;
		clamped_base.base_color = binding(
			texture_owners.base_color, PbrMaterial::TextureSampler::clamp_to_edge());

		const std::array proofs{
			TextureProofCase{ "base-colour texture", glm::vec4(1.0f), 0.0f, 0.55f, base_only },
			TextureProofCase{ "packed metallic-roughness texture", glm::vec4(0.75f), 1.0f, 1.0f, metallic_roughness_only },
			TextureProofCase{
				"normal texture scale 1 with non-uniform transform",
				glm::vec4(0.55f, 0.65f, 0.9f, 1.0f),
				0.0f,
				0.5f,
				normal_only,
				1.0f,
				false,
				true },
			TextureProofCase{ "all textures combined", glm::vec4(1.0f), 1.0f, 1.0f, combined },
			TextureProofCase{ "texture-factor multiplication", glm::vec4(0.35f, 1.0f, 0.35f, 1.0f), 0.45f, 0.65f, combined },
			TextureProofCase{ "normal texture scale 2", glm::vec4(0.55f, 0.65f, 0.9f, 1.0f), 0.0f, 0.5f, normal_only, 2.0f },
			TextureProofCase{ "repeat sampler", glm::vec4(1.0f), 0.0f, 0.55f, base_only, 1.0f, true },
			TextureProofCase{ "clamp-to-edge sampler", glm::vec4(1.0f), 0.0f, 0.55f, clamped_base, 1.0f, true },
		};

		const auto static_cube = make_texture_cube(engine, 1.0f);
		const auto repeating_static_cube = make_texture_cube(engine, 3.0f);
		const auto non_uniform_static_cube = make_texture_cube(engine, 1.0f, true);
		const auto skinned_cube = make_skinned_texture_cube(engine, static_cube);
		const auto repeating_skinned_cube = make_skinned_texture_cube(
			engine, repeating_static_cube);
		const auto non_uniform_skinned_cube = make_skinned_texture_cube(
			engine, non_uniform_static_cube);
		Bone root;
		root.name = "Textured PBR proof root";
		const SkeletonID skeleton = engine.get_ecs().add_skeleton({ root });
		auto& root_pose = engine.get_ecs().get_skeletal_component(skeleton)
			.get_bone_local_transform(0);
		root_pose.set_pos(glm::vec3(0.08f, 0.0f, 0.0f));
		root_pose.set_orient(glm::angleAxis(Maths::PI * 0.08f, Maths::forward_vec));

		for (size_t column = 0; column < proofs.size(); ++column)
		{
			const float x = (static_cast<float>(column) - 3.5f) * 1.65f;
			const auto& proof = proofs[column];
			attach_texture_proof_mesh(
				engine,
				proof.non_uniform_transform ? non_uniform_static_cube
					: proof.repeated_uvs ? repeating_static_cube : static_cube,
				std::nullopt,
				texture_owners,
				proof,
				glm::vec3(x, 5.55f, 0.8f));
			attach_texture_proof_mesh(
				engine,
				proof.non_uniform_transform ? non_uniform_skinned_cube
					: proof.repeated_uvs ? repeating_skinned_cube : skinned_cube,
				skeleton,
				texture_owners,
				proof,
				glm::vec3(x, 6.75f, 0.8f));
		}
	}

	static MaterialHandle make_proof_texture(
		GameEngine& engine,
		const std::string& name,
		const ETextureSemantic semantic,
		const uint32_t width,
		const std::initializer_list<uint8_t> rgba)
	{
		if (rgba.size() % (width * 4) != 0)
			throw std::runtime_error("PBR proof texture dimensions are invalid");
		std::vector<std::byte> pixels;
		pixels.reserve(rgba.size());
		for (const uint8_t channel : rgba)
			pixels.push_back(static_cast<std::byte>(channel));
		auto texture = std::make_unique<TextureMaterial>();
		texture->data = std::make_unique<OwnedTextureData>(std::move(pixels));
		texture->data_len = rgba.size();
		texture->width = width;
		texture->height = static_cast<uint32_t>(rgba.size() / (width * 4));
		texture->channels = 4;
		texture->mip_sizes = { rgba.size() };
		texture->source = name;
		texture->semantic = semantic;
		return engine.get_ecs().get_material_system().add(std::move(texture));
	}

	static void attach_stage3_card(
		GameEngine& engine,
		const MeshHandle& mesh,
		const std::vector<MaterialHandle>& proof_textures,
		const std::string& name,
		const glm::vec3 position,
		const float y_rotation,
		const glm::vec4 base_color,
		PbrMaterial::TextureSlots slots,
		const PbrMaterial::Properties properties)
	{
		const bool textured = slots.base_color || slots.metallic_roughness
			|| slots.normal || slots.emissive;
		auto material = engine.get_ecs().get_material_system().add(
			std::make_unique<PbrMaterial>(
				base_color, 0.0f, 0.7f, slots, 1.0f, properties));
		std::vector<MaterialHandle> owners{ std::move(material) };
		for (const auto& binding : {
			slots.base_color, slots.metallic_roughness, slots.normal, slots.emissive })
		{
			if (!binding)
				continue;
			const auto owner = std::ranges::find_if(
				proof_textures,
				[&binding](const MaterialHandle& candidate)
				{ return candidate->get_id() == binding->texture; });
			if (owner != proof_textures.end())
				owners.push_back(*owner);
		}

		Renderable renderable{
			.name = name,
			.pipeline_render_type = textured ? ERenderType::STANDARD : ERenderType::COLOR,
			.mesh_owner = mesh,
			.material_owners = std::move(owners),
		};
		auto& object = engine.spawn_object<Object>();
		object.set_name(name);
		engine.attach_renderable(object.get_id(), std::move(renderable));
		auto& transform = engine.get_ecs().get_transformation(object.get_id());
		transform.set_position(position);
		transform.set_rotation(glm::angleAxis(y_rotation, Maths::up_vec));
		transform.set_scale(glm::vec3(0.85f));
	}

	static void spawn_stage3_material_proof(GameEngine& engine)
	{
		// Intentionally tiny, procedural RGBA fixtures make alpha values reviewable
		// and avoid image-tool colour conversion or premultiplication.
		const auto cutout = make_proof_texture(
			engine, "procedural cutout", ETextureSemantic::BASE_COLOR, 4, {
				40, 220, 70, 255, 40, 220, 70, 255, 20, 80, 30, 0,   20, 80, 30, 0,
				40, 220, 70, 255, 20, 80, 30, 0,   20, 80, 30, 0,   40, 220, 70, 255,
				20, 80, 30, 0,   40, 220, 70, 255, 40, 220, 70, 255, 20, 80, 30, 0,
				20, 80, 30, 0,   20, 80, 30, 0,   40, 220, 70, 255, 40, 220, 70, 255,
			});
		const auto straight_alpha = make_proof_texture(
			engine, "procedural straight alpha", ETextureSemantic::BASE_COLOR, 4, {
				255, 40, 20, 48,  255, 40, 20, 96,  255, 40, 20, 160, 255, 40, 20, 224,
				20, 180, 255, 224, 20, 180, 255, 160, 20, 180, 255, 96, 20, 180, 255, 48,
				255, 40, 20, 48,  255, 40, 20, 96,  255, 40, 20, 160, 255, 40, 20, 224,
				20, 180, 255, 224, 20, 180, 255, 160, 20, 180, 255, 96, 20, 180, 255, 48,
			});
		const auto emissive = make_proof_texture(
			engine, "procedural emissive", ETextureSemantic::EMISSIVE, 4, {
				255, 20, 10, 255, 255, 120, 10, 255, 20, 40, 255, 255, 20, 255, 120, 255,
				20, 40, 255, 255, 20, 255, 120, 255, 255, 20, 10, 255, 255, 120, 10, 255,
				255, 20, 10, 255, 255, 120, 10, 255, 20, 40, 255, 255, 20, 255, 120, 255,
				20, 40, 255, 255, 20, 255, 120, 255, 255, 20, 10, 255, 255, 120, 10, 255,
			});
		const std::vector proof_textures{ cutout, straight_alpha, emissive };
		const auto binding = [](const MaterialHandle& owner)
		{
			return PbrMaterial::TextureBinding{
				owner->get_id(), PbrMaterial::TextureSampler::clamp_to_edge() };
		};
		const auto texture_quad = engine.get_ecs().get_mesh_system().add(
			MeshFactory::quad(MeshFactory::EVertexType::TEXTURE));
		const auto color_quad = engine.get_ecs().get_mesh_system().add(
			MeshFactory::quad(MeshFactory::EVertexType::COLOR));

		PbrMaterial::TextureSlots cutout_slot{ .base_color = binding(cutout) };
		attach_stage3_card(engine, texture_quad, proof_textures, "Masked alpha cutout",
			glm::vec3(-5.0f, 7.65f, 3.0f), 0.0f, glm::vec4(1.0f), cutout_slot,
			PbrMaterial::Properties{
				.alpha_mode = EAlphaMode::MASK, .alpha_cutoff = 0.5f });

		const PbrMaterial::Properties crossed_properties{
			.alpha_mode = EAlphaMode::MASK,
			.alpha_cutoff = 0.5f,
			.double_sided = true,
		};
		attach_stage3_card(engine, texture_quad, proof_textures, "Crossed double-sided card A",
			glm::vec3(-3.0f, 7.65f, 3.0f), Maths::PI * 0.25f,
			glm::vec4(1.0f), cutout_slot, crossed_properties);
		attach_stage3_card(engine, texture_quad, proof_textures, "Crossed double-sided card B",
			glm::vec3(-3.0f, 7.65f, 3.0f), -Maths::PI * 0.25f,
			glm::vec4(1.0f), cutout_slot, crossed_properties);

		attach_stage3_card(engine, color_quad, proof_textures, "Factor emissive",
			glm::vec3(-1.0f, 7.65f, 3.0f), 0.0f,
			glm::vec4(0.03f, 0.03f, 0.03f, 1.0f), {},
			PbrMaterial::Properties{
				.emissive_factor = glm::vec3(1.0f, 0.2f, 0.02f) });

		PbrMaterial::TextureSlots emissive_slot{ .emissive = binding(emissive) };
		attach_stage3_card(engine, texture_quad, proof_textures, "Textured emissive",
			glm::vec3(1.0f, 7.65f, 3.0f), 0.0f,
			glm::vec4(0.03f, 0.03f, 0.03f, 1.0f), emissive_slot,
			PbrMaterial::Properties{ .emissive_factor = glm::vec3(1.0f) });

		const PbrMaterial::Properties blend_properties{ .alpha_mode = EAlphaMode::BLEND };
		attach_stage3_card(engine, color_quad, proof_textures, "Straight-alpha factor overlap",
			glm::vec3(2.88f, 7.65f, 3.1f), -0.08f,
			glm::vec4(0.15f, 0.8f, 1.0f, 0.45f), {}, blend_properties);
		PbrMaterial::TextureSlots alpha_slot{ .base_color = binding(straight_alpha) };
		attach_stage3_card(engine, texture_quad, proof_textures, "Straight-alpha textured overlap",
			glm::vec3(3.12f, 7.65f, 2.85f), 0.08f,
			glm::vec4(1.0f), alpha_slot, blend_properties);

		PbrMaterial::TextureSlots combined_slots{
			.base_color = binding(straight_alpha),
			.emissive = binding(emissive),
		};
		attach_stage3_card(engine, texture_quad, proof_textures,
			"Double-sided emissive blend combination",
			glm::vec3(5.0f, 7.65f, 3.0f), Maths::PI * 0.18f,
			glm::vec4(1.0f), combined_slots,
			PbrMaterial::Properties{
				.alpha_mode = EAlphaMode::BLEND,
				.double_sided = true,
				.emissive_factor = glm::vec3(0.75f),
			});
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
