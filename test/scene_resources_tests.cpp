#include <entity_component_system/ecs.hpp>
#include <renderable/material.hpp>
#include <renderable/composited_texture_material.hpp>
#include <renderable/mesh_factory.hpp>
#include <resource_loader/resource_loader.hpp>
#include <serialization/resource_provenance.hpp>
#include <serialization/scene_resources.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <concepts>
#include <cstring>
#include <filesystem>

namespace
{
class SceneResourcesTests : public testing::Test
{
protected:
	void SetUp() override
	{
		directory =
			std::filesystem::temp_directory_path() /
			("krisp_scene_resources_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
		std::filesystem::create_directory(directory);
	}

	void TearDown() override
	{
		std::filesystem::remove_all(directory);
		ResourceProvenance::clear();
	}

	std::filesystem::path directory;
};

template<typename MeshType> void expect_same_mesh(const Mesh &expected, const Mesh &actual)
{
	const auto &expected_typed = dynamic_cast<const MeshType &>(expected);
	const auto &actual_typed = dynamic_cast<const MeshType &>(actual);
	if constexpr (std::same_as<MeshType, SkinnedMesh>)
	{
		ASSERT_EQ(actual_typed.get_vertices().size(), expected_typed.get_vertices().size());
		for (std::size_t index = 0; index < actual_typed.get_vertices().size(); ++index)
		{
			const auto &lhs = actual_typed.get_vertices()[index];
			const auto &rhs = expected_typed.get_vertices()[index];
			EXPECT_EQ(lhs.bone_ids, rhs.bone_ids);
			EXPECT_EQ(lhs.bone_weights, rhs.bone_weights);
			EXPECT_EQ(lhs.pos, rhs.pos);
			EXPECT_EQ(lhs.normal, rhs.normal);
			EXPECT_EQ(lhs.texCoord, rhs.texCoord);
			EXPECT_EQ(lhs.tangent, rhs.tangent);
		}
	}
	else
	{
		EXPECT_EQ(actual_typed.get_vertices(), expected_typed.get_vertices());
	}
	EXPECT_EQ(actual_typed.get_indices(), expected_typed.get_indices());
}
} // namespace

TEST_F(SceneResourcesTests, round_trips_all_generated_mesh_layouts_and_deduplicates_references)
{
	ECS source;
	auto color = source.get_mesh_system().add(MeshFactory::cube(MeshFactory::EVertexType::COLOR));
	auto textured = source.get_mesh_system().add(MeshFactory::cube(MeshFactory::EVertexType::TEXTURE));
	SkinnedVertices vertices(3);
	vertices[0].pos = {0.0f, 0.0f, 0.0f};
	vertices[1].pos = {1.0f, 0.0f, 0.0f};
	vertices[2].pos = {0.0f, 1.0f, 0.0f};
	for (auto &vertex : vertices)
	{
		vertex.normal = {0.0f, 0.0f, 1.0f};
		vertex.bone_ids = glm::vec4(0.0f);
		vertex.bone_weights = {1.0f, 0.0f, 0.0f, 0.0f};
	}
	// Unequal vertex/index counts verify that the binary header fields are
	// encoded independently.
	auto skinned =
		source.get_mesh_system().add(std::make_unique<SkinnedMesh>(vertices, VertexIndices{0, 1, 2, 0, 2, 1}));

	Serializer document;
	SceneResourceWriter writer(document, source, directory);
	auto references = document.sequence("references");
	for (const auto &mesh : {color, textured, skinned})
		writer.write_mesh_reference(references.append_map(), mesh->get_id());
	writer.write_mesh_reference(references.append_map(), color->get_id());

	const auto saved = Deserializer::parse(document.emit());
	EXPECT_EQ(saved.child("resources").child("meshes").elements().size(), 3);
	ECS restored;
	SceneResourceReader reader(restored, directory);
	reader.prepare(saved);
	const auto restored_references = saved.child("references").elements();
	const auto restored_color = reader.read_mesh_reference(restored_references[0]);
	const auto restored_textured = reader.read_mesh_reference(restored_references[1]);
	const auto restored_skinned = reader.read_mesh_reference(restored_references[2]);
	const auto duplicate_color = reader.read_mesh_reference(restored_references[3]);

	expect_same_mesh<ColorMesh>(color->get(), restored_color->get());
	expect_same_mesh<TexMesh>(textured->get(), restored_textured->get());
	expect_same_mesh<SkinnedMesh>(skinned->get(), restored_skinned->get());
	EXPECT_EQ(restored_color, duplicate_color);
}

TEST_F(SceneResourcesTests, round_trips_pbr_material_and_raw_texture)
{
	ECS source;
	auto texture = std::make_unique<TextureMaterial>();
	texture->width = 2;
	texture->height = 1;
	texture->channels = 4;
	texture->data_len = 8;
	texture->mip_sizes = {8};
	texture->semantic = ETextureSemantic::EMISSIVE;
	texture->source = "generated";
	const std::vector<std::byte> pixels{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4},
	                                    std::byte{5}, std::byte{6}, std::byte{7}, std::byte{8}};
	texture->data = std::make_unique<OwnedTextureData>(pixels);
	auto texture_owner = source.get_material_system().add(std::move(texture));
	PbrMaterial::TextureSlots slots;
	slots.emissive = PbrMaterial::TextureBinding{
		.texture = texture_owner->get_id(),
		.sampler = PbrMaterial::TextureSampler::clamp_to_edge(
			PbrMaterial::TextureSampler::MipmapMode::NEAREST),
	};
	auto pbr = std::make_unique<PbrMaterial>(
		glm::vec4(0.1f, 0.2f, 0.3f, 0.4f), 0.6f, 0.7f, slots, 0.35f,
		PbrMaterial::Properties{
			.alpha_mode = EAlphaMode::BLEND,
			.alpha_cutoff = 1.25f,
			.double_sided = true,
			.emissive_factor = glm::vec3(0.15f, 0.25f, 0.35f),
		});
	auto pbr_owner = source.get_material_system().add(std::move(pbr));

	Serializer document;
	SceneResourceWriter writer(document, source, directory);
	auto references = document.sequence("references");
	writer.write_material_reference(references.append_map(), pbr_owner->get_id());
	writer.write_material_reference(references.append_map(), texture_owner->get_id());
	const auto saved = Deserializer::parse(document.emit());
	ECS restored;
	SceneResourceReader reader(restored, directory);
	reader.prepare(saved);
	const auto restored_references = saved.child("references").elements();
	const auto restored_pbr = reader.read_material_reference(restored_references[0]);
	const auto restored_texture = reader.read_material_reference(restored_references[1]);
	const auto &pbr_value = dynamic_cast<const PbrMaterial &>(restored_pbr->get());
	const auto &texture_value = dynamic_cast<const TextureMaterial &>(restored_texture->get());

	EXPECT_EQ(pbr_value.data.base_color_factor, glm::vec4(0.1f, 0.2f, 0.3f, 0.4f));
	EXPECT_EQ(pbr_value.data.metallic_factor, 0.6f);
	EXPECT_EQ(pbr_value.data.roughness_factor, 0.7f);
	EXPECT_EQ(pbr_value.data.normal_scale, 0.35f);
	EXPECT_EQ(pbr_value.properties.alpha_mode, EAlphaMode::BLEND);
	EXPECT_FLOAT_EQ(pbr_value.properties.alpha_cutoff, 1.25f);
	EXPECT_TRUE(pbr_value.properties.double_sided);
	EXPECT_EQ(pbr_value.properties.emissive_factor, glm::vec3(0.15f, 0.25f, 0.35f));
	EXPECT_EQ(pbr_value.data.emissive_factor, pbr_value.properties.emissive_factor);
	ASSERT_TRUE(pbr_value.textures.emissive.has_value());
	EXPECT_EQ(pbr_value.textures.emissive->texture, restored_texture->get_id());
	EXPECT_EQ(
		pbr_value.textures.emissive->sampler,
		PbrMaterial::TextureSampler::clamp_to_edge(
			PbrMaterial::TextureSampler::MipmapMode::NEAREST));
	EXPECT_EQ(texture_value.semantic, ETextureSemantic::EMISSIVE);
	EXPECT_EQ(texture_value.source, "generated");
	ASSERT_EQ(texture_value.data_len, pixels.size());
	EXPECT_EQ(std::memcmp(texture_value.data->get(), pixels.data(), pixels.size()), 0);
}

TEST_F(SceneResourcesTests, round_trips_imported_pbr_property_and_emissive_texture_overrides)
{
	ECS source;
	const auto loaded = ResourceLoader::load_model(source, "static_mesh_textured.gltf");
	const auto base_owner = loaded.meshes[0].renderables[0].material_owners.front();
	const auto& base = dynamic_cast<const PbrMaterial&>(base_owner->get());
	const auto* provenance = ResourceProvenance::material(base_owner->get_id());
	ASSERT_NE(provenance, nullptr);
	const auto source_provenance = *provenance;
	auto emissive_owner = ResourceLoader::fetch_texture(
		source.get_material_system(), "texture.jpg", ETextureSemantic::EMISSIVE);
	auto slots = base.textures;
	slots.emissive = PbrMaterial::TextureBinding{
		.texture = emissive_owner->get_id(),
		.sampler = PbrMaterial::TextureSampler::clamp_to_edge(),
	};
	const PbrMaterial::Properties properties{
		.alpha_mode = EAlphaMode::MASK,
		.alpha_cutoff = 1.5f,
		.double_sided = true,
		.emissive_factor = glm::vec3(0.2f, 0.4f, 0.6f),
	};
	auto override_owner = source.get_material_system().add(std::make_unique<PbrMaterial>(
		base.data.base_color_factor, base.data.metallic_factor, base.data.roughness_factor,
		slots, base.data.normal_scale, properties));
	ResourceProvenance::register_material_override(override_owner->get_id(), {
		.source = source_provenance,
		.original = {
			.base_color_factor = base.data.base_color_factor,
			.metallic_factor = base.data.metallic_factor,
			.roughness_factor = base.data.roughness_factor,
			.normal_scale = base.data.normal_scale,
			.properties = base.properties,
			.textures = base.textures,
		},
		.alpha_mode = properties.alpha_mode,
		.alpha_cutoff = properties.alpha_cutoff,
		.double_sided = properties.double_sided,
		.emissive_factor = properties.emissive_factor,
		.emissive_texture = PbrTextureOverride{
			.mode = PbrTextureOverride::Mode::Replaced,
			.texture = emissive_owner->get_id(),
			.sampler = PbrMaterial::TextureSampler::clamp_to_edge(),
		},
	});

	Serializer document;
	SceneResourceWriter writer(document, source, directory);
	writer.write_material_reference(document.map("material"), override_owner->get_id());
	const auto saved = Deserializer::parse(document.emit());

	ECS restored;
	SceneResourceReader reader(restored, directory);
	reader.prepare(saved);
	const auto restored_source = ResourceLoader::load_model(restored, "static_mesh_textured.gltf");
	const auto restored_owner = reader.read_material_reference(saved.child("material"));
	const auto& restored_material = dynamic_cast<const PbrMaterial&>(restored_owner->get());
	EXPECT_EQ(restored_material.properties.alpha_mode, EAlphaMode::MASK);
	EXPECT_FLOAT_EQ(restored_material.properties.alpha_cutoff, 1.5f);
	EXPECT_TRUE(restored_material.properties.double_sided);
	EXPECT_EQ(restored_material.properties.emissive_factor, glm::vec3(0.2f, 0.4f, 0.6f));
	ASSERT_TRUE(restored_material.textures.emissive.has_value());
	const auto& restored_texture = dynamic_cast<const TextureMaterial&>(
		restored.get_material_system().get(restored_material.textures.emissive->texture));
	EXPECT_EQ(restored_texture.semantic, ETextureSemantic::EMISSIVE);
	EXPECT_EQ(restored_material.textures.emissive->sampler,
		PbrMaterial::TextureSampler::clamp_to_edge());
	(void)restored_source;
}

TEST_F(SceneResourcesTests, round_trips_composited_texture_recipe_without_pixel_payload)
{
	ECS source;
	std::vector<MaterialHandle> texture_owners;
	for (int index = 0; index < 3; ++index)
	{
		auto texture = std::make_unique<TextureMaterial>();
		texture->width = 2;
		texture->height = 1;
		texture->channels = 4;
		texture->data_len = 8;
		texture->mip_sizes = { 8 };
		texture->data = std::make_unique<OwnedTextureData>(std::vector<std::byte>(8));
		texture_owners.push_back(source.get_material_system().add(std::move(texture)));
	}
	std::vector<TextureCompositionLayer> layers{
		{ .source = texture_owners[0] },
		{ .source = texture_owners[1], .centre = { 0.25f, 0.75f },
		  .scale = { 0.5f, 0.25f }, .rotation_radians = 0.3f,
		  .tint = { 0.2f, 0.4f, 0.6f }, .opacity = 0.7f },
		{ .source = texture_owners[2] },
	};
	auto composition = source.get_material_system().add(
		std::make_unique<CompositedTextureMaterial>(2, 1, std::move(layers)));

	Serializer document;
	SceneResourceWriter writer(document, source, directory);
	writer.write_material_reference(document.map("material"), composition->get_id());
	const auto saved = Deserializer::parse(document.emit());
	EXPECT_EQ(saved.child("resources").child("materials").elements().size(), 4);
	EXPECT_EQ(std::ranges::count_if(std::filesystem::directory_iterator(directory), [](const auto& entry) {
		return entry.path().extension() == ".dat";
	}), 3);

	ECS restored;
	SceneResourceReader reader(restored, directory);
	reader.prepare(saved);
	const auto restored_owner = reader.read_material_reference(saved.child("material"));
	const auto& restored_composition = dynamic_cast<const CompositedTextureMaterial&>(
		restored_owner->get());
	ASSERT_EQ(restored_composition.layers.size(), 3);
	EXPECT_EQ(restored_composition.width, 2u);
	EXPECT_EQ(restored_composition.height, 1u);
	EXPECT_EQ(restored_composition.layers[1].centre, glm::vec2(0.25f, 0.75f));
	EXPECT_EQ(restored_composition.layers[1].scale, glm::vec2(0.5f, 0.25f));
	EXPECT_FLOAT_EQ(restored_composition.layers[1].rotation_radians, 0.3f);
	EXPECT_EQ(restored_composition.layers[1].tint, glm::vec3(0.2f, 0.4f, 0.6f));
	EXPECT_FLOAT_EQ(restored_composition.layers[1].opacity, 0.7f);
}

TEST_F(SceneResourcesTests, rejects_truncated_mesh_data)
{
	ECS source;
	auto mesh = source.get_mesh_system().add(MeshFactory::cube());
	Serializer document;
	SceneResourceWriter writer(document, source, directory);
	writer.write_mesh_reference(document.map("mesh"), mesh->get_id());
	const auto saved = Deserializer::parse(document.emit());
	const auto file = saved.child("resources").child("meshes").elements()[0].read<std::string>("file");
	std::filesystem::resize_file(directory / file, 8);
	ECS restored;
	SceneResourceReader reader(restored, directory);

	EXPECT_THROW(reader.prepare(saved), SerializationError);
}
