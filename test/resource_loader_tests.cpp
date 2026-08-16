#include "test_helper.hpp"

#include <resource_loader/resource_loader.hpp>
#include <utility.hpp>
#include <entity_component_system/ecs.hpp>
#include <entity_component_system/material_system.hpp>
#include <entity_component_system/mesh_system.hpp>
#include <renderable/material_factory.hpp>
#include <serialization/resource_provenance.hpp>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <tiny_gltf.h>

#include <cmath>
#include <fstream>

namespace
{
ECS general_loader_ecs;
}

class ResourceLoaderECS : public testing::Test
{
public:
	ResourceLoaderECS()
	{
		model = ResourceLoader::load_model(ecs, model_path);
	}

	glm::vec3 apply_transform(const glm::mat4& transform, const glm::vec3& v)
	{
		return glm::vec3(transform * glm::vec4(v, 1.0f));
	}

	const std::vector<Bone>& get_bones()
	{
		const auto skeleton_id = model.meshes[0].skeleton_id.value();
		const auto& skeleton = ecs.get_skeletal_component(skeleton_id);
		return skeleton.get_bones();
	}

	// extremely simple skinned model, contains a 2x2x4 (width,depth,height) cube mesh with 5 bones
	// the bones look a bit like this, they are perfectly axis aligned
	/*
		 |
		_|_
		 |
	*/
	const std::string model_path = "simple_test_model.gltf";
	ECS ecs;
	ResourceLoader::LoadedModel model;
	glm::vec3 v1 = {0.0f, 0.0f, 0.0f};
	glm::vec3 v2 = {1.0f, 1.0f, 0.0f};
};

TEST(ResourceLoaderOwnership, skeletal_state_is_registered_in_the_supplied_ecs)
{
	ECS target;
	ECS unrelated;
	const auto model = ResourceLoader::load_model(
		target, "simple_test_model.gltf");
	const SkeletonID skeleton = model.meshes.front().skeleton_id.value();

	EXPECT_NO_THROW(target.get_skeletal_component(skeleton));
	EXPECT_THROW(unrelated.get_skeletal_component(skeleton), std::out_of_range);
}

TEST(ResourceLoaderOwnership, texture_loading_registers_with_the_supplied_material_store)
{
	ECS first;
	ECS second;
	const auto first_owner = ResourceLoader::fetch_texture(
		first.get_material_system(), "texture.jpg");
	const auto second_owner = ResourceLoader::fetch_texture(
		second.get_material_system(), "texture.jpg");

	EXPECT_NE(first_owner, second_owner);
	EXPECT_TRUE(first.get_material_system().owns(first_owner));
	EXPECT_TRUE(second.get_material_system().owns(second_owner));
	EXPECT_FALSE(first.get_material_system().owns(second_owner));
}

namespace
{
std::string encode_base64(const std::vector<unsigned char>& bytes)
{
	constexpr std::string_view ALPHABET =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	std::string encoded;
	encoded.reserve((bytes.size() + 2) / 3 * 4);
	for (size_t offset = 0; offset < bytes.size(); offset += 3)
	{
		const uint32_t value = static_cast<uint32_t>(bytes[offset]) << 16
			| (offset + 1 < bytes.size() ? static_cast<uint32_t>(bytes[offset + 1]) << 8 : 0)
			| (offset + 2 < bytes.size() ? static_cast<uint32_t>(bytes[offset + 2]) : 0);
		encoded.push_back(ALPHABET[(value >> 18) & 0x3f]);
		encoded.push_back(ALPHABET[(value >> 12) & 0x3f]);
		encoded.push_back(offset + 1 < bytes.size() ? ALPHABET[(value >> 6) & 0x3f] : '=');
		encoded.push_back(offset + 2 < bytes.size() ? ALPHABET[value & 0x3f] : '=');
	}
	return encoded;
}

class GeneratedDDS
{
public:
	GeneratedDDS(
		const uint32_t width,
		const uint32_t height,
		const uint32_t mip_levels,
		const uint32_t four_cc,
		const std::optional<size_t> payload_size = std::nullopt)
	{
		static uint32_t sequence = 0;
		path = Utility::get_top_level_path()/"test/data"
			/ fmt::format("krisp_test_{}.dds", sequence++);
		contents.resize(128, 0);
		const auto write_u32 = [this](const size_t offset, const uint32_t value)
		{
			for (size_t byte = 0; byte < 4; ++byte)
				contents[offset + byte] = static_cast<unsigned char>(value >> (byte * 8));
		};
		write_u32(0, 0x20534444); // "DDS "
		write_u32(4, 124);
		write_u32(8, 0x000A1007);
		write_u32(12, height);
		write_u32(16, width);
		write_u32(28, mip_levels);
		write_u32(76, 32);
		write_u32(80, 0x4); // DDPF_FOURCC
		write_u32(84, four_cc);
		write_u32(108, 0x00401008);

		size_t expected_payload = 0;
		uint32_t mip_width = width;
		uint32_t mip_height = height;
		for (uint32_t mip = 0; mip < mip_levels; ++mip)
		{
			expected_payload += static_cast<size_t>((mip_width + 3) / 4)
				* static_cast<size_t>((mip_height + 3) / 4) * 16;
			mip_width = std::max(1u, mip_width / 2);
			mip_height = std::max(1u, mip_height / 2);
		}
		contents.resize(128 + payload_size.value_or(expected_payload), 0x7f);
		std::ofstream output(path, std::ios::binary);
		output.write(reinterpret_cast<const char*>(contents.data()), contents.size());
	}

	~GeneratedDDS()
	{
		std::error_code error;
		std::filesystem::remove(path, error);
	}

	std::filesystem::path path;
	std::vector<unsigned char> contents;
	std::string filename() const { return path.filename().string(); }
};

void add_msft_texture_dds(
	nlohmann::json& document,
	const nlohmann::json& image,
	const bool retain_fallback = true)
{
	document["extensionsUsed"] = nlohmann::json::array({ "MSFT_texture_dds" });
	document["images"].push_back(image);
	const auto image_index = document["images"].size() - 1;
	document["textures"][0]["extensions"]["MSFT_texture_dds"] = {
		{ "source", image_index },
	};
	if (!retain_fallback)
	{
		document["textures"][0].erase("source");
		document["extensionsRequired"] = nlohmann::json::array({ "MSFT_texture_dds" });
	}
}

class GeneratedGlbDDS
{
public:
	explicit GeneratedGlbDDS(const GeneratedDDS& dds)
	{
		static uint32_t sequence = 0;
		path = Utility::get_top_level_path() / "test/data"
			/ fmt::format("krisp_test_dds_{}.glb", sequence++);

		tinygltf::TinyGLTF io;
		tinygltf::Model model;
		std::string error;
		std::string warning;
		if (!io.LoadASCIIFromFile(
				&model, &error, &warning, Utility::get_model("static_mesh_textured.gltf").string()))
			throw std::runtime_error("Unable to load glTF test template: " + error);

		auto& buffer = model.buffers.at(0);
		while (buffer.data.size() % 4 != 0)
			buffer.data.push_back(0);
		tinygltf::BufferView view;
		view.buffer = 0;
		view.byteOffset = buffer.data.size();
		view.byteLength = dds.contents.size();
		buffer.data.insert(buffer.data.end(), dds.contents.begin(), dds.contents.end());
		model.bufferViews.push_back(view);

		tinygltf::Image image;
		image.bufferView = static_cast<int>(model.bufferViews.size() - 1);
		image.mimeType = "image/vnd-ms.dds";
		model.images.push_back(std::move(image));
		model.extensionsUsed.push_back("MSFT_texture_dds");
		tinygltf::Value::Object extension;
		extension.emplace("source", tinygltf::Value(static_cast<int>(model.images.size() - 1)));
		model.textures.at(0).extensions.emplace(
			"MSFT_texture_dds", tinygltf::Value(std::move(extension)));
		buffer.uri.clear();
		if (!io.WriteGltfSceneToFile(&model, path.string(), false, false, false, true))
			throw std::runtime_error("Unable to write GLB DDS test resource");
	}

	~GeneratedGlbDDS()
	{
		std::error_code error;
		std::filesystem::remove(path, error);
	}

	std::filesystem::path path;
	std::string filename() const { return path.filename().string(); }
};

class MutatedGltf
{
public:
	explicit MutatedGltf(
		const std::function<void(nlohmann::json&)>& mutate,
		std::string_view template_filename = "static_mesh_textured.gltf",
		const bool strip_textures = true)
	{
		static uint32_t sequence = 0;
		path = Utility::get_top_level_path()/"test/data"
			/ fmt::format("krisp_test_accessor_{}.gltf", sequence++);
		std::ifstream input(Utility::get_model(template_filename));
		nlohmann::json document;
		input >> document;
		if (strip_textures && template_filename == "static_mesh_textured.gltf")
			document["materials"][0]["pbrMetallicRoughness"].erase("baseColorTexture");
		mutate(document);
		std::ofstream output(path);
		output << document;
	}

	~MutatedGltf()
	{
		std::error_code error;
		std::filesystem::remove(path, error);
	}

	std::filesystem::path path;
	std::string filename() const { return path.filename().string(); }
};

class TemporaryExternalJpeg
{
public:
	TemporaryExternalJpeg()
	{
		static uint32_t sequence = 0;
		path = Utility::get_top_level_path()/"test/data"
			/ fmt::format("krisp_test_external_{}.jpg", sequence++);
		std::filesystem::copy_file(
			Utility::get_top_level_path()/"resources/default/textures/texture.jpg",
			path,
			std::filesystem::copy_options::overwrite_existing);
	}

	~TemporaryExternalJpeg()
	{
		std::error_code error;
		std::filesystem::remove(path, error);
	}

	std::filesystem::path path;
	std::string filename() const { return path.filename().string(); }
};
}

TEST(ResourceLoaderErrors, public_load_apis_report_typed_errors)
{
	const auto missing_model = "does_not_exist.glb";
	const auto missing_texture = "does_not_exist.png";

	EXPECT_THROW(ResourceLoader::load_model(general_loader_ecs, missing_model), ResourceLoadError);
	EXPECT_THROW(ResourceLoader::fetch_texture(general_loader_ecs.get_material_system(), missing_texture), ResourceLoadError);
	EXPECT_THROW(ResourceLoader::load_model(general_loader_ecs, "/tmp/model.glb"), ResourceLoadError);
	EXPECT_THROW(ResourceLoader::fetch_texture(general_loader_ecs.get_material_system(), "../texture.png"), ResourceLoadError);
}

TEST(ResourceLoaderErrors, rejects_accessor_data_outside_its_buffer_view)
{
	MutatedGltf resource([](nlohmann::json& document)
	{
		document["bufferViews"][0]["byteLength"] = 4;
	});
	EXPECT_THROW(ResourceLoader::load_model(general_loader_ecs, resource.filename()), ResourceLoadError);
}

TEST(ResourceLoaderErrors, rejects_accessor_stride_smaller_than_its_element)
{
	MutatedGltf resource([](nlohmann::json& document)
	{
		document["bufferViews"][0]["byteStride"] = 4;
	});
	EXPECT_THROW(ResourceLoader::load_model(general_loader_ecs, resource.filename()), ResourceLoadError);
}

TEST(ResourceLoaderCoordinates, converts_static_mesh_basis)
{
	ECS ecs;
	MutatedGltf resource([](nlohmann::json&) {});
	const auto model = ResourceLoader::load_model(ecs, resource.filename());
	const auto& renderable = model.meshes[0].renderables[0];
	const auto& mesh = dynamic_cast<const ColorMesh&>(
		ecs.get_mesh_system().get(renderable.mesh_owner->get_id()));

	ASSERT_EQ(mesh.get_vertices().size(), 3);
	EXPECT_TRUE(glm_equal(mesh.get_vertices()[0].pos, glm::vec3(0.0f, 0.0f, 0.0f)));
	EXPECT_TRUE(glm_equal(mesh.get_vertices()[1].pos, glm::vec3(-1.0f, 0.0f, 0.0f)));
	EXPECT_TRUE(glm_equal(mesh.get_vertices()[2].pos, glm::vec3(-0.5f, 1.0f, 0.0f)));
	EXPECT_EQ(mesh.get_indices(), (std::vector<uint32_t>{ 0, 2, 1 }));
}

TEST(ResourceLoaderCoordinates, converts_static_node_translation_and_rotation)
{
	MutatedGltf resource([](nlohmann::json& document)
	{
		document["nodes"][0]["translation"] = { 2.0, 0.0, 0.0 };
		document["nodes"][0]["rotation"] = {
			0.0, 0.0, std::sin(Maths::PI / 4.0f), std::cos(Maths::PI / 4.0f) };
	});
	ECS ecs;
	const auto model = ResourceLoader::load_model(ecs, resource.filename());
	const auto& transform = model.meshes[0].renderables[0].local_transform;

	EXPECT_TRUE(glm_equal(transform.get_pos(), glm::vec3(-2.0f, 0.0f, 0.0f)));
	EXPECT_TRUE(glm_equal(
		transform.get_orient(), glm::angleAxis(-Maths::PI / 2.0f, Maths::forward_vec)));
}

TEST(ResourceLoaderCoordinates, converts_and_composes_nested_matrix_nodes)
{
	MutatedGltf resource([](nlohmann::json& document)
	{
		document["scenes"][0]["nodes"] = { 0 };
		document["nodes"] = {
			{
				{ "children", { 1 } },
				{ "matrix", {
					0.0, 1.0, 0.0, 0.0,
					-1.0, 0.0, 0.0, 0.0,
					0.0, 0.0, 1.0, 0.0,
					2.0, 0.0, 0.0, 1.0 } }
			},
			{
				{ "mesh", 0 },
				{ "translation", { 1.0, 0.0, 0.0 } }
			}
		};
	});
	ECS ecs;
	const auto model = ResourceLoader::load_model(ecs, resource.filename());
	const auto& transform = model.meshes[0].renderables[0].local_transform;

	EXPECT_TRUE(glm_equal(transform.get_pos(), glm::vec3(-2.0f, 1.0f, 0.0f)));
	EXPECT_TRUE(glm_equal(
		transform.get_orient(), glm::angleAxis(-Maths::PI / 2.0f, Maths::forward_vec)));
}

TEST(ResourceLoaderCoordinates, converts_skinned_mesh_and_bind_pose_basis)
{
	ECS ecs;
	const auto model = ResourceLoader::load_model(ecs, "gltf_basis_skinned.gltf");
	const auto& loaded_mesh = model.meshes[0];
	const auto& renderable = loaded_mesh.renderables[0];
	const auto& mesh = dynamic_cast<const SkinnedMesh&>(
		ecs.get_mesh_system().get(renderable.mesh_owner->get_id()));
	const auto& bone = ecs.get_skeletal_component(*loaded_mesh.skeleton_id).get_bones()[0];

	EXPECT_EQ(mesh.get_indices(), (std::vector<uint32_t>{ 0, 2, 1 }));
	EXPECT_TRUE(glm_equal(mesh.get_vertices()[1].pos, glm::vec3(-1.0f, 0.0f, 0.0f)));
	EXPECT_TRUE(glm_equal(mesh.get_vertices()[0].normal, glm::vec3(-1.0f, 0.0f, 0.0f)));
	EXPECT_EQ(mesh.get_vertices()[0].bone_ids, glm::vec4(0.0f));
	EXPECT_EQ(mesh.get_vertices()[0].bone_weights, glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
	EXPECT_TRUE(glm_equal(bone.relative_transform.get_pos(), glm::vec3(-2.0f, 0.0f, 0.0f)));
	EXPECT_TRUE(glm_equal(bone.inverse_bind_pose.get_pos(), glm::vec3(-2.0f, 0.0f, 0.0f)));
}

TEST(ResourceLoaderCoordinates, converts_cubic_animation_values_and_tangents)
{
	ECS ecs;
	const auto model = ResourceLoader::load_model(ecs, "gltf_basis_skinned.gltf");
	const auto loaded = ResourceLoader::load_animations(
		ecs, "gltf_basis_cubic_animation.gltf", *model.meshes[0].skeleton_id);
	ASSERT_EQ(loaded.animations.size(), 1);
	const auto& animation = ecs.get_skeletal_animations().at(loaded.animations[0]).bone_animations[0];

	ASSERT_EQ(animation.translation_track.interpolation, BoneAnimation::Interpolation::CUBIC_SPLINE);
	ASSERT_EQ(animation.translation_track.keys.size(), 2);
	EXPECT_TRUE(glm_equal(animation.translation_track.keys[0].in_tangent, glm::vec3(-1.0f, 2.0f, 3.0f)));
	EXPECT_TRUE(glm_equal(animation.translation_track.keys[0].value, glm::vec3(-2.0f, 3.0f, 4.0f)));
	EXPECT_TRUE(glm_equal(animation.translation_track.keys[0].out_tangent, glm::vec3(-5.0f, 6.0f, 7.0f)));

	ASSERT_EQ(animation.rotation_track.interpolation, BoneAnimation::Interpolation::CUBIC_SPLINE);
	ASSERT_EQ(animation.rotation_track.keys.size(), 2);
	EXPECT_TRUE(glm_equal(animation.rotation_track.keys[0].in_tangent, glm::vec4(0.1f, -0.2f, -0.3f, 0.4f)));
	EXPECT_TRUE(glm_equal(animation.rotation_track.keys[0].out_tangent, glm::vec4(0.5f, -0.6f, -0.7f, 0.8f)));
	EXPECT_TRUE(glm_equal(
		animation.rotation_track.keys[1].value,
		glm::vec4(0.0f, 0.0f, -std::sqrt(0.5f), std::sqrt(0.5f))));
}

TEST(ResourceLoaderMaterials, imports_exact_pbr_factors)
{
	MutatedGltf resource([](nlohmann::json& document)
	{
		auto& material = document["materials"][0];
		material["name"] = "Exact factors";
		material["pbrMetallicRoughness"]["baseColorFactor"] = { 0.125, 0.25, 0.5, 0.75 };
		material["pbrMetallicRoughness"]["metallicFactor"] = 0.375;
		material["pbrMetallicRoughness"]["roughnessFactor"] = 0.625;
	}, "skinned_color.gltf");
	ECS ecs;
	const auto model = ResourceLoader::load_model(ecs, resource.filename());
	const auto& renderable = model.meshes[0].renderables[0];
	const auto& owner = renderable.material_owners[0];
	const auto& material = dynamic_cast<const PbrMaterial&>(
		ecs.get_material_system().get(owner->get_id()));
	EXPECT_TRUE(glm_equal(material.data.base_color_factor, glm::vec4(0.125f, 0.25f, 0.5f, 0.75f)));
	EXPECT_FLOAT_EQ(material.data.metallic_factor, 0.375f);
	EXPECT_FLOAT_EQ(material.data.roughness_factor, 0.625f);
	EXPECT_FLOAT_EQ(renderable.opacity, 1.0f);
}

TEST(ResourceLoaderMaterials, imports_core_alpha_sidedness_and_emissive_properties)
{
	MutatedGltf resource([](nlohmann::json& document)
	{
		auto& material = document["materials"][0];
		material["alphaMode"] = "MASK";
		material["alphaCutoff"] = 1.25;
		material["doubleSided"] = true;
		material["emissiveFactor"] = { 0.125, 0.25, 0.5 };
		material["emissiveTexture"] = { { "index", 0 } };
	}, "static_mesh_textured.gltf", false);
	ECS ecs;
	const auto model = ResourceLoader::load_model(ecs, resource.filename());
	const auto& renderable = model.meshes[0].renderables[0];
	const auto& material = dynamic_cast<const PbrMaterial&>(
		renderable.material_owners.front()->get());
	EXPECT_EQ(material.properties.alpha_mode, EAlphaMode::MASK);
	EXPECT_FLOAT_EQ(material.properties.alpha_cutoff, 1.25f);
	EXPECT_TRUE(material.properties.double_sided);
	EXPECT_EQ(material.properties.emissive_factor, glm::vec3(0.125f, 0.25f, 0.5f));
	EXPECT_EQ(material.data.emissive_factor, material.properties.emissive_factor);
	const TextureMaterial* emissive = nullptr;
	for (const auto& owner : renderable.material_owners)
		if (const auto* texture = dynamic_cast<const TextureMaterial*>(&owner->get());
			texture && texture->semantic == ETextureSemantic::EMISSIVE)
			emissive = texture;
	ASSERT_NE(emissive, nullptr);
	ASSERT_TRUE(material.textures.emissive.has_value());
	EXPECT_EQ(material.textures.emissive->texture, emissive->get_id());
	EXPECT_TRUE(material.has_textures());
}

TEST(ResourceLoaderMaterials, imports_blend_alpha_mode)
{
	MutatedGltf resource([](nlohmann::json& document)
	{
		document["materials"][0]["alphaMode"] = "BLEND";
	});
	ECS ecs;
	const auto model = ResourceLoader::load_model(ecs, resource.filename());
	const auto& material = dynamic_cast<const PbrMaterial&>(
		model.meshes[0].renderables[0].material_owners.front()->get());
	EXPECT_EQ(material.properties.alpha_mode, EAlphaMode::BLEND);
}

TEST(ResourceLoaderMaterials, primitives_without_material_use_gltf_pbr_defaults)
{
	MutatedGltf resource([](nlohmann::json& document)
	{
		document.erase("materials");
		document["meshes"][0]["primitives"][0].erase("material");
	});
	ECS ecs;
	const auto model = ResourceLoader::load_model(ecs, resource.filename());
	const auto& owner = model.meshes[0].renderables[0].material_owners[0];
	const auto& material = dynamic_cast<const PbrMaterial&>(
		ecs.get_material_system().get(owner->get_id()));
	EXPECT_TRUE(glm_equal(material.data.base_color_factor, glm::vec4(1.0f)));
	EXPECT_FLOAT_EQ(material.data.metallic_factor, 1.0f);
	EXPECT_FLOAT_EQ(material.data.roughness_factor, 1.0f);
	EXPECT_EQ(material.properties.alpha_mode, EAlphaMode::OPAQUE);
	EXPECT_FLOAT_EQ(material.properties.alpha_cutoff, 0.5f);
	EXPECT_FALSE(material.properties.double_sided);
	EXPECT_EQ(material.properties.emissive_factor, glm::vec3(0.0f));
	EXPECT_FALSE(material.textures.emissive.has_value());
}

TEST(ResourceLoaderMaterials, rejects_unsupported_reachable_material_features_with_material_context)
{
	using Mutation = std::function<void(nlohmann::json&)>;
	const std::vector<std::pair<std::string, Mutation>> cases{
		{ "occlusionTexture", [](auto& material) { material["occlusionTexture"] = { { "index", 0 } }; } },
		{ "emissiveFactor", [](auto& material) { material["emissiveFactor"] = { 0.0, 1.1, 0.0 }; } },
		{ "alphaMode", [](auto& material) { material["alphaMode"] = "INVALID"; } },
		{ "alphaCutoff", [](auto& material) { material["alphaCutoff"] = -0.1; } },
		{ "KHR_materials_specular", [](auto& material) {
			material["extensions"]["KHR_materials_specular"] = { { "specularFactor", 0.5 } }; } },
		{ "metallicFactor outside", [](auto& material) {
			material["pbrMetallicRoughness"]["metallicFactor"] = 1.1; } },
	};

	for (const auto& [feature, mutate] : cases)
	{
		SCOPED_TRACE(feature);
		MutatedGltf resource([&](nlohmann::json& document)
		{
			auto& material = document["materials"][0];
			material["name"] = "Unsupported material";
			mutate(material);
		});
		try
		{
			ECS ecs;
			(void)ResourceLoader::load_model(ecs, resource.filename());
			FAIL() << "Expected ResourceLoadError";
		}
		catch (const ResourceLoadError& error)
		{
			const std::string message(error.what());
			EXPECT_NE(message.find("Unsupported material"), std::string::npos);
			EXPECT_NE(message.find(feature), std::string::npos);
		}
	}
}

TEST(ResourceLoaderMaterials, warns_for_unsupported_unused_material_declarations)
{
	MutatedGltf resource([](nlohmann::json& document)
	{
		document["materials"].push_back({
			{ "name", "Unused extension material" },
			{ "extensions", { { "KHR_materials_specular", { { "specularFactor", 0.5 } } } } },
		});
	});
	ECS ecs;
	const auto model = ResourceLoader::load_model(ecs, resource.filename());
	ASSERT_EQ(model.warnings.size(), 1);
	EXPECT_NE(model.warnings.front().message.find("Unused extension material"), std::string::npos);
	EXPECT_NE(model.warnings.front().message.find("KHR_materials_specular"), std::string::npos);

	ResourceLoader::LoadOptions strict;
	strict.strict = true;
	EXPECT_THROW(ResourceLoader::load_model(ecs, resource.filename(), strict), ResourceLoadError);
}

// test loading .gltf file with bones into std::vector<Bone>
TEST_F(ResourceLoaderECS, load_bones)
{
	ASSERT_EQ(model.meshes.size(), 1);
	ASSERT_EQ(model.meshes[0].renderables.size(), 1);
	const auto& renderable = model.meshes[0].renderables[0];
	ASSERT_EQ(renderable.pipeline_render_type, ERenderType::SKINNED_COLOR);
	const auto skeleton_id = model.meshes[0].skeleton_id.value();
	const auto& skeleton = ecs.get_skeletal_component(skeleton_id);
	ASSERT_EQ(skeleton.get_bones().size(), 5);
}

TEST_F(ResourceLoaderECS, bone_relative_transforms)
{
	// root bone
	ASSERT_TRUE(glm_equal(get_bones()[0].relative_transform.get_mat4(), Maths::identity_mat));

	// mid bone
	ASSERT_TRUE(glm_equal(
		get_bones()[1].relative_transform.get_mat4(),
		glm::translate(Maths::identity_mat, Maths::up_vec)));

	// tip bone
	ASSERT_TRUE(glm_equal(
		get_bones()[2].relative_transform.get_mat4(),
		glm::translate(Maths::identity_mat, Maths::up_vec)));

	// right bone
	ASSERT_TRUE(glm_equal(get_bones()[3].relative_transform.get_pos(), Maths::up_vec));
	ASSERT_TRUE(glm_equal(get_bones()[3].relative_transform.get_scale(), Maths::identity_vec));
	ASSERT_TRUE(glm_equal(
		get_bones()[3].relative_transform.get_orient(),
		glm::angleAxis(-Maths::PI / 2.0f, Maths::forward_vec)));

	// left bone
	ASSERT_TRUE(glm_equal(get_bones()[4].relative_transform.get_pos(), Maths::up_vec));
	ASSERT_TRUE(glm_equal(get_bones()[4].relative_transform.get_scale(), Maths::identity_vec));
	ASSERT_TRUE(glm_equal(
		get_bones()[4].relative_transform.get_orient(),
		Maths::zRot90));
}

TEST_F(ResourceLoaderECS, model_load_ignores_animations)
{
	EXPECT_TRUE(ecs.get_skeletal_animations().empty());
	EXPECT_TRUE(std::ranges::any_of(model.warnings, [](const auto& warning)
	{
		return warning.message.find("animations were ignored") != std::string::npos;
	}));

	ResourceLoader::LoadOptions strict_options;
	strict_options.strict = true;
	ECS strict_ecs;
	EXPECT_THROW(ResourceLoader::load_model(strict_ecs, model_path, strict_options), ResourceLoadError);
}

TEST_F(ResourceLoaderECS, explicitly_loaded_animations_preserve_tracks)
{
	const auto loaded = ResourceLoader::load_animations(ecs, model_path, model.meshes[0].skeleton_id.value());
	ASSERT_EQ(loaded.animations.size(), 1);
	std::vector<BoneAnimation> bone_animations =
		ecs.get_skeletal_animations().at(loaded.animations[0]).bone_animations;
	ASSERT_EQ(bone_animations.size(), 5);
	ASSERT_EQ(
		bone_animations[0].translation_track.interpolation,
		BoneAnimation::Interpolation::STEP);
	// check animation scale is never modified
	for (const auto& bone_animation : bone_animations)
	{
		ASSERT_TRUE(glm_equal(bone_animation.base_transform.get_scale(), Maths::identity_vec));
		for (const auto& key : bone_animation.scale_track.keys)
			ASSERT_TRUE(glm_equal(key.value, Maths::identity_vec));
	}

	// check positions preserve the base pose and per-channel values
	const auto pos_checker = [](const BoneAnimation& animation, const glm::vec3& expected_pos)
	{
		if (!glm_equal(animation.base_transform.get_pos(), expected_pos))
			return false;
		for (const auto& key : animation.translation_track.keys)
			if (!glm_equal(key.value, expected_pos))
				return false;
		return true;
	};

	const auto key_quat = [](const BoneAnimation::TrackKey<glm::vec4>& key)
	{
		return glm::quat(key.value.w, key.value.x, key.value.y, key.value.z);
	};

	// check rotations preserve the base pose and per-channel values
	const auto quat_checker = [](const BoneAnimation& animation, const glm::quat& expected_quat)
	{
		if (!glm_equal(animation.base_transform.get_orient(), expected_quat))
			return false;
		for (const auto& key : animation.rotation_track.keys)
			if (!glm_equal(glm::quat(key.value.w, key.value.x, key.value.y, key.value.z), expected_quat))
				return false;
		return true;
	};

	// root bone
	ASSERT_TRUE(pos_checker(bone_animations[0], Maths::zero_vec));
	ASSERT_TRUE(quat_checker(bone_animations[0], Maths::identity_quat));

	// mid bone
	ASSERT_TRUE(pos_checker(bone_animations[1], Maths::up_vec));
	ASSERT_EQ(bone_animations[1].rotation_track.keys.size(), 4);
	ASSERT_TRUE(glm_equal(key_quat(bone_animations[1].rotation_track.keys[0]), Maths::identity_quat));
	ASSERT_TRUE(glm_equal(key_quat(bone_animations[1].rotation_track.keys[1]),
		glm::angleAxis(-Maths::PI/4.0f, Maths::forward_vec)));
	ASSERT_TRUE(glm_equal(key_quat(bone_animations[1].rotation_track.keys[2]),
		glm::angleAxis(Maths::PI/4.0f, Maths::forward_vec)));
	ASSERT_TRUE(glm_equal(key_quat(bone_animations[1].rotation_track.keys[3]), Maths::identity_quat));

	// tip bone
	ASSERT_TRUE(pos_checker(bone_animations[2], Maths::up_vec));
	ASSERT_EQ(bone_animations[2].rotation_track.keys.size(), 4);
	ASSERT_TRUE(glm_equal(key_quat(bone_animations[2].rotation_track.keys[0]), Maths::identity_quat));
	ASSERT_TRUE(glm_equal(key_quat(bone_animations[2].rotation_track.keys[1]),
		glm::angleAxis(-Maths::PI/8.0f, Maths::forward_vec)));
	ASSERT_TRUE(glm_equal(key_quat(bone_animations[2].rotation_track.keys[2]),
		glm::angleAxis(Maths::PI/8.0f, Maths::forward_vec)));
	ASSERT_TRUE(glm_equal(key_quat(bone_animations[2].rotation_track.keys[3]), Maths::identity_quat));

	// right bone
	ASSERT_TRUE(pos_checker(bone_animations[3], Maths::up_vec));
	ASSERT_TRUE(quat_checker(
		bone_animations[3], glm::angleAxis(-Maths::PI / 2.0f, Maths::forward_vec)));

	// left bone
	ASSERT_TRUE(pos_checker(bone_animations[4], Maths::up_vec));
	ASSERT_TRUE(quat_checker(bone_animations[4], Maths::zRot90));
}

TEST_F(ResourceLoaderECS, standalone_animation_file_remaps_reordered_joints_by_name)
{
	const auto skeleton_id = model.meshes[0].skeleton_id.value();
	const auto path = "standalone_animation.gltf";
	const size_t animations_before = ecs.get_skeletal_animations().size();
	const auto loaded = ResourceLoader::load_animations(ecs, path, skeleton_id);
	ASSERT_EQ(loaded.animations.size(), 2);
	EXPECT_EQ(ecs.get_skeletal_animations().size(), animations_before + 2);

	std::optional<AnimationID> root_move;
	std::optional<AnimationID> mid_turn;
	for (const auto id : loaded.animations)
	{
		const auto& animation = ecs.get_skeletal_animations().at(id);
		EXPECT_EQ(animation.source, "standalone_animation.gltf");
		EXPECT_TRUE(ecs.is_animation_compatible(skeleton_id, id));
		if (animation.name == "RootMove") root_move = id;
		if (animation.name == "MidTurn") mid_turn = id;
	}
	ASSERT_TRUE(root_move);
	ASSERT_TRUE(mid_turn);

	const auto& root_animation = ecs.get_skeletal_animations().at(*root_move);
	ASSERT_EQ(root_animation.bone_animations[0].translation_track.keys.size(), 2);
	EXPECT_TRUE(glm_equal(
		root_animation.bone_animations[0].translation_track.keys.back().value,
		glm::vec3(-2.0f, 0.0f, 0.0f)));
	EXPECT_TRUE(root_animation.bone_animations[1].translation_track.keys.empty());

	const auto& mid_animation = ecs.get_skeletal_animations().at(*mid_turn);
	EXPECT_EQ(
		mid_animation.bone_animations[1].rotation_track.interpolation,
		BoneAnimation::Interpolation::STEP);
	EXPECT_TRUE(mid_animation.bone_animations[0].rotation_track.keys.empty());
	ASSERT_EQ(mid_animation.bone_animations[1].rotation_track.keys.size(), 2);
	const auto& rotation = mid_animation.bone_animations[1].rotation_track.keys.back().value;
	EXPECT_TRUE(glm_equal(
		glm::quat(rotation.w, rotation.x, rotation.y, rotation.z),
		glm::angleAxis(-Maths::PI / 2.0f, Maths::forward_vec)));

	const auto reloaded = ResourceLoader::load_animations(ecs, path, skeleton_id);
	ASSERT_EQ(reloaded.animations.size(), 2);
	EXPECT_NE(reloaded.animations[0], loaded.animations[0]);
	EXPECT_EQ(ecs.get_skeletal_animations().size(), animations_before + 4);
}

TEST_F(ResourceLoaderECS, standalone_animation_rejects_incompatible_or_incomplete_rigs_atomically)
{
	const auto valid_skeleton = model.meshes[0].skeleton_id.value();
	const auto animation_path = "standalone_animation.gltf";
	const size_t animations_before = ecs.get_skeletal_animations().size();

	auto incompatible_bones = get_bones();
	incompatible_bones[1].parent_node = Bone::NO_PARENT;
	const auto incompatible_skeleton = ecs.add_skeleton(incompatible_bones);
	EXPECT_THROW(ResourceLoader::load_animations(ecs, animation_path, incompatible_skeleton), ResourceLoadError);

	auto duplicate_bones = get_bones();
	duplicate_bones[1].name = duplicate_bones[0].name;
	const auto duplicate_skeleton = ecs.add_skeleton(duplicate_bones);
	EXPECT_THROW(ResourceLoader::load_animations(ecs, animation_path, duplicate_skeleton), ResourceLoadError);

	EXPECT_THROW(ResourceLoader::load_animations(ecs,
		"standalone_animation_no_skin.gltf", valid_skeleton), ResourceLoadError);
	EXPECT_THROW(ResourceLoader::load_animations(ecs,
		"standalone_animation_no_clips.gltf", valid_skeleton), ResourceLoadError);
	EXPECT_EQ(ecs.get_skeletal_animations().size(), animations_before);
}

TEST_F(ResourceLoaderECS, animation_playback_validates_rigs_and_replaces_active_clip)
{
	const auto skeleton_id = model.meshes[0].skeleton_id.value();
	const auto loaded = ResourceLoader::load_animations(ecs,
		"standalone_animation.gltf", skeleton_id);
	ASSERT_EQ(loaded.animations.size(), 2);

	std::optional<AnimationID> root_move;
	std::optional<AnimationID> mid_turn;
	for (const auto id : loaded.animations)
	{
		const auto& animation = ecs.get_skeletal_animations().at(id);
		if (animation.name == "RootMove") root_move = id;
		if (animation.name == "MidTurn") mid_turn = id;
	}
	ASSERT_TRUE(root_move);
	ASSERT_TRUE(mid_turn);
	ecs.play_animation(skeleton_id, *root_move, true);
	ASSERT_TRUE(ecs.get_animation_playback(skeleton_id));
	EXPECT_EQ(ecs.get_animation_playback(skeleton_id)->animation_id, *root_move);
	EXPECT_TRUE(ecs.get_animation_playback(skeleton_id)->looping);
	ecs.process(0.5f);
	EXPECT_FLOAT_EQ(ecs.get_animation_playback(skeleton_id)->elapsed_secs, 0.5f);
	EXPECT_FLOAT_EQ(ecs.get_animation_playback(skeleton_id)->duration_secs, 1.0f);
	EXPECT_TRUE(glm_equal(
		ecs.get_skeletal_component(skeleton_id).get_bones()[0].relative_transform.get_pos(),
		glm::vec3(-1.0f, 0.0f, 0.0f)));

	ecs.set_animation_paused(skeleton_id, true);
	EXPECT_TRUE(ecs.get_animation_playback(skeleton_id)->paused);
	ecs.seek_animation(skeleton_id, 0.25f);
	EXPECT_FLOAT_EQ(ecs.get_animation_playback(skeleton_id)->elapsed_secs, 0.25f);
	EXPECT_NEAR(
		ecs.get_skeletal_component(skeleton_id).get_bones()[0].relative_transform.get_pos().x,
		-0.5f, 0.0001f);
	ecs.seek_animation(skeleton_id, -1.0f);
	EXPECT_FLOAT_EQ(ecs.get_animation_playback(skeleton_id)->elapsed_secs, 0.0f);
	ecs.seek_animation(skeleton_id, 2.0f);
	EXPECT_FLOAT_EQ(ecs.get_animation_playback(skeleton_id)->elapsed_secs, 1.0f);
	ecs.seek_animation(skeleton_id, 0.5f);
	ecs.process(0.25f);
	EXPECT_TRUE(glm_equal(
		ecs.get_skeletal_component(skeleton_id).get_bones()[0].relative_transform.get_pos(),
		glm::vec3(-1.0f, 0.0f, 0.0f)));
	ecs.step_animation(skeleton_id, 1.0f / 30.0f);
	const float stepped_forward =
		ecs.get_skeletal_component(skeleton_id).get_bones()[0].relative_transform.get_pos().x;
	EXPECT_LT(stepped_forward, -1.0f);
	ecs.step_animation(skeleton_id, -1.0f / 30.0f);
	EXPECT_NEAR(
		ecs.get_skeletal_component(skeleton_id).get_bones()[0].relative_transform.get_pos().x,
		-1.0f, 0.0001f);

	ecs.set_animation_looping(skeleton_id, false);
	ecs.set_animation_paused(skeleton_id, false);
	ecs.process(1.0f);
	EXPECT_TRUE(glm_equal(
		ecs.get_skeletal_component(skeleton_id).get_bones()[0].relative_transform.get_pos(),
		Maths::zero_vec));

	ecs.play_animation(skeleton_id, *root_move);
	ecs.process(0.5f);
	ecs.stop_animation(skeleton_id);
	EXPECT_FALSE(ecs.get_animation_playback(skeleton_id));
	EXPECT_TRUE(glm_equal(
		ecs.get_skeletal_component(skeleton_id).get_bones()[0].relative_transform.get_pos(),
		Maths::zero_vec));
	ecs.process(0.5f);
	EXPECT_TRUE(glm_equal(
		ecs.get_skeletal_component(skeleton_id).get_bones()[0].relative_transform.get_pos(),
		Maths::zero_vec));

	ecs.play_animation(skeleton_id, *mid_turn);
	EXPECT_TRUE(glm_equal(
		ecs.get_skeletal_component(skeleton_id).get_bones()[0].relative_transform.get_pos(),
		Maths::zero_vec));

	auto incompatible_bones = get_bones();
	incompatible_bones[0].name = "different-root";
	const auto incompatible_skeleton = ecs.add_skeleton(incompatible_bones);
	EXPECT_FALSE(ecs.play_animation(incompatible_skeleton, *root_move));
	ecs.process(2.0f);
}

TEST(BoneAnimationInterpolation, step_holds_and_cubic_spline_uses_tangents)
{
	BoneAnimation animation;
	animation.animation_start_secs = 0.0f;
	animation.animation_end_secs = 1.0f;
	animation.base_transform.set_scale(glm::vec3(1.0f));
	animation.translation_track.interpolation = BoneAnimation::Interpolation::CUBIC_SPLINE;
	animation.translation_track.keys = {
		{ 0.0f, glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(2.0f, 0.0f, 0.0f) },
		{ 1.0f, glm::vec3(2.0f, 0.0f, 0.0f), glm::vec3(0.0f), glm::vec3(0.0f) },
	};
	animation.scale_track.interpolation = BoneAnimation::Interpolation::STEP;
	animation.scale_track.keys = {
		{ 0.0f, glm::vec3(1.0f), glm::vec3(0.0f), glm::vec3(0.0f) },
		{ 1.0f, glm::vec3(3.0f), glm::vec3(0.0f), glm::vec3(0.0f) },
	};

	Maths::Transform result;
	ASSERT_TRUE(animation.get_transform(0.5f, result));
	EXPECT_TRUE(glm_equal(result.get_pos(), glm::vec3(1.25f, 0.0f, 0.0f)));
	EXPECT_TRUE(glm_equal(result.get_scale(), glm::vec3(1.0f)));

	ASSERT_TRUE(animation.get_transform(1.0f, result));
	EXPECT_TRUE(glm_equal(result.get_pos(), glm::vec3(2.0f, 0.0f, 0.0f)));
	EXPECT_TRUE(glm_equal(result.get_scale(), glm::vec3(3.0f)));
}

TEST(ResourceLoaderTextures, fetch_same_texture_path_twice_registers_independent_materials)
{
	const std::string texture_path = "texture.jpg";

	const auto first = ResourceLoader::fetch_texture(general_loader_ecs.get_material_system(), texture_path);
	const auto second = ResourceLoader::fetch_texture(general_loader_ecs.get_material_system(), texture_path);

	ASSERT_NE(first, second);
	EXPECT_EQ(first.use_count(), 1);
	EXPECT_EQ(second.use_count(), 1);
	EXPECT_EQ(dynamic_cast<const TextureMaterial&>(
		general_loader_ecs.get_material_system().get(first->get_id())).source, texture_path);
	EXPECT_EQ(dynamic_cast<const TextureMaterial&>(
		general_loader_ecs.get_material_system().get(second->get_id())).source, texture_path);
}

TEST(ResourceLoaderTextures, loads_texture_variants_by_semantic)
{
	constexpr uint32_t DXT5 = 0x35545844;
	GeneratedDDS dds(4, 4, 1, DXT5);
	const auto texture_path = dds.filename();
	auto base_color = ResourceLoader::fetch_texture(general_loader_ecs.get_material_system(),
		texture_path, ETextureSemantic::BASE_COLOR);
	const auto normal = ResourceLoader::fetch_texture(general_loader_ecs.get_material_system(),
		texture_path, ETextureSemantic::NORMAL);
	EXPECT_NE(base_color->get_id(), normal->get_id());
	EXPECT_EQ(dynamic_cast<const TextureMaterial&>(
		general_loader_ecs.get_material_system().get(base_color->get_id())).semantic,
		ETextureSemantic::BASE_COLOR);
	EXPECT_EQ(dynamic_cast<const TextureMaterial&>(
		general_loader_ecs.get_material_system().get(normal->get_id())).semantic,
		ETextureSemantic::NORMAL);

	const MaterialID released = base_color->get_id();
	base_color.reset();
	EXPECT_FALSE(general_loader_ecs.get_material_system().contains(released));
	const auto reloaded = ResourceLoader::fetch_texture(general_loader_ecs.get_material_system(),
		texture_path, ETextureSemantic::BASE_COLOR);
	EXPECT_NE(reloaded->get_id(), released);
	EXPECT_EQ(reloaded.use_count(), 1);
}

TEST(ResourceLoaderTextures, loads_generated_bc3_dds_with_complete_mip_chain)
{
	constexpr uint32_t DXT5 = 0x35545844;
	GeneratedDDS dds(8, 8, 4, DXT5);
	const auto material = ResourceLoader::fetch_texture(general_loader_ecs.get_material_system(), dds.filename());
	const auto& loaded = static_cast<const TextureMaterial&>(
		general_loader_ecs.get_material_system().get(material->get_id()));

	EXPECT_EQ(loaded.width, 8);
	EXPECT_EQ(loaded.height, 8);
	EXPECT_EQ(loaded.channels, 4);
	EXPECT_EQ(loaded.format, ETextureFormat::BC3);
	EXPECT_EQ(loaded.data_len, 112);
	EXPECT_EQ(loaded.mip_sizes, (std::vector<size_t>{ 64, 16, 16, 16 }));
	ASSERT_NE(loaded.data, nullptr);
	EXPECT_EQ(std::to_integer<unsigned char>(*loaded.data->get()), 0x7f);
}

TEST(ResourceLoaderTextures, rejects_unsupported_or_truncated_dds_files)
{
	constexpr uint32_t DXT1 = 0x31545844;
	constexpr uint32_t DXT5 = 0x35545844;
	GeneratedDDS unsupported(4, 4, 1, DXT1);
	GeneratedDDS truncated(8, 8, 4, DXT5, 111);

	EXPECT_THROW(ResourceLoader::fetch_texture(general_loader_ecs.get_material_system(), unsupported.filename()), ResourceLoadError);
	EXPECT_THROW(ResourceLoader::fetch_texture(general_loader_ecs.get_material_system(), truncated.filename()), ResourceLoadError);
}

namespace
{
const PbrMaterial* find_pbr_material(const Renderable& renderable)
{
	for (const auto& owner : renderable.material_owners)
		if (const auto* material = dynamic_cast<const PbrMaterial*>(&owner->get()))
			return material;
	return nullptr;
}

const TextureMaterial* find_texture_material(
	const Renderable& renderable,
	const ETextureSemantic semantic)
{
	for (const auto& owner : renderable.material_owners)
		if (const auto* material = dynamic_cast<const TextureMaterial*>(&owner->get());
			material && material->semantic == semantic)
			return material;
	return nullptr;
}
}

TEST(ResourceLoaderTexturedPbr, imports_external_msft_texture_dds_with_authored_mips)
{
	constexpr uint32_t DXT5 = 0x35545844;
	GeneratedDDS dds(8, 8, 4, DXT5);
	MutatedGltf resource([&](nlohmann::json& document)
	{
		add_msft_texture_dds(document, { { "uri", dds.filename() } });
		document["samplers"] = nlohmann::json::array({ {
			{ "magFilter", 9729 }, { "minFilter", 9985 },
			{ "wrapS", 10497 }, { "wrapT", 10497 },
		} });
		document["textures"][0]["sampler"] = 0;
	}, "static_mesh_textured.gltf", false);

	ECS ecs;
	const auto model = ResourceLoader::load_model(ecs, resource.filename());
	const auto& renderable = model.meshes[0].renderables[0];
	const auto* texture = find_texture_material(renderable, ETextureSemantic::BASE_COLOR);
	const auto* pbr = find_pbr_material(renderable);
	ASSERT_NE(texture, nullptr);
	ASSERT_NE(pbr, nullptr);
	ASSERT_TRUE(pbr->textures.base_color.has_value());
	EXPECT_EQ(texture->format, ETextureFormat::BC3);
	EXPECT_EQ(texture->source, dds.filename());
	EXPECT_EQ(texture->mip_sizes, (std::vector<size_t>{ 64, 16, 16, 16 }));
	EXPECT_EQ(pbr->textures.base_color->sampler, PbrMaterial::TextureSampler::repeat(
		PbrMaterial::TextureSampler::MipmapMode::NEAREST));
}

TEST(ResourceLoaderTexturedPbr, imports_required_data_uri_msft_texture_dds_without_fallback)
{
	constexpr uint32_t DXT5 = 0x35545844;
	GeneratedDDS dds(4, 4, 3, DXT5);
	const auto uri = "data:application/octet-stream;base64," + encode_base64(dds.contents);
	MutatedGltf resource([&](nlohmann::json& document)
	{
		add_msft_texture_dds(document, { { "uri", uri } }, false);
		document["samplers"] = nlohmann::json::array({ {
			{ "magFilter", 9729 }, { "minFilter", 9987 },
			{ "wrapS", 33071 }, { "wrapT", 33071 },
		} });
		document["textures"][0]["sampler"] = 0;
	}, "static_mesh_textured.gltf", false);

	ECS ecs;
	const auto model = ResourceLoader::load_model(ecs, resource.filename());
	const auto& renderable = model.meshes[0].renderables[0];
	const auto* texture = find_texture_material(renderable, ETextureSemantic::BASE_COLOR);
	const auto* pbr = find_pbr_material(renderable);
	ASSERT_NE(texture, nullptr);
	ASSERT_NE(pbr, nullptr);
	ASSERT_TRUE(pbr->textures.base_color.has_value());
	EXPECT_EQ(texture->format, ETextureFormat::BC3);
	EXPECT_EQ(texture->mip_sizes, (std::vector<size_t>{ 16, 16, 16 }));
	EXPECT_EQ(pbr->textures.base_color->sampler,
		PbrMaterial::TextureSampler::clamp_to_edge());
}

TEST(ResourceLoaderTexturedPbr, imports_glb_buffer_view_msft_texture_dds)
{
	constexpr uint32_t DXT5 = 0x35545844;
	GeneratedDDS dds(4, 4, 3, DXT5);
	GeneratedGlbDDS resource(dds);

	ECS ecs;
	const auto model = ResourceLoader::load_model(ecs, resource.filename());
	const auto* texture = find_texture_material(
		model.meshes[0].renderables[0], ETextureSemantic::BASE_COLOR);
	ASSERT_NE(texture, nullptr);
	EXPECT_EQ(texture->format, ETextureFormat::BC3);
	EXPECT_EQ(texture->mip_sizes, (std::vector<size_t>{ 16, 16, 16 }));
}

TEST(ResourceLoaderTexturedPbr, rejects_invalid_msft_texture_dds_declarations)
{
	constexpr uint32_t DXT5 = 0x35545844;
	GeneratedDDS dds(4, 4, 1, DXT5);
	MutatedGltf missing_used([&](nlohmann::json& document)
	{
		add_msft_texture_dds(document, { { "uri", dds.filename() } });
		document.erase("extensionsUsed");
	}, "static_mesh_textured.gltf", false);
	MutatedGltf missing_required([&](nlohmann::json& document)
	{
		add_msft_texture_dds(document, { { "uri", dds.filename() } }, false);
		document.erase("extensionsRequired");
	}, "static_mesh_textured.gltf", false);

	ECS ecs;
	EXPECT_THROW(ResourceLoader::load_model(ecs, missing_used.filename()), ResourceLoadError);
	EXPECT_THROW(ResourceLoader::load_model(ecs, missing_required.filename()), ResourceLoadError);
}

TEST(ResourceLoaderTexturedPbr, imports_static_buffer_view_png_and_texcoord_zero)
{
	ECS ecs;
	const auto model = ResourceLoader::load_model(ecs, "static_mesh_textured.gltf");
	ASSERT_EQ(model.meshes.size(), 1);
	ASSERT_EQ(model.meshes[0].renderables.size(), 1);
	const auto& renderable = model.meshes[0].renderables[0];
	EXPECT_EQ(renderable.pipeline_render_type, ERenderType::STANDARD);
	const auto* pbr = find_pbr_material(renderable);
	ASSERT_NE(pbr, nullptr);
	const auto* base_color = find_texture_material(renderable, ETextureSemantic::BASE_COLOR);
	ASSERT_NE(base_color, nullptr);
	ASSERT_TRUE(pbr->textures.base_color.has_value());
	EXPECT_EQ(pbr->textures.base_color->texture, base_color->get_id());
	EXPECT_FALSE(pbr->textures.metallic_roughness.has_value());
	EXPECT_FALSE(pbr->textures.normal.has_value());
	EXPECT_EQ(base_color->width, 2u);
	EXPECT_EQ(base_color->height, 2u);
	const auto& mesh = dynamic_cast<const TexMesh&>(
		ecs.get_mesh_system().get(renderable.mesh_owner->get_id()));
	ASSERT_EQ(mesh.get_vertices().size(), 3);
	EXPECT_EQ(mesh.get_vertices()[0].texCoord, glm::vec2(0.0f, 0.0f));
	EXPECT_EQ(mesh.get_vertices()[1].texCoord, glm::vec2(1.0f, 0.0f));
	EXPECT_EQ(mesh.get_vertices()[2].texCoord, glm::vec2(0.5f, 1.0f));
}

TEST(ResourceLoaderTexturedPbr, imports_all_optional_maps_factors_normal_scale_and_clamp_sampler)
{
	MutatedGltf resource([](nlohmann::json& document)
	{
		auto& pbr = document["materials"][0]["pbrMetallicRoughness"];
		pbr["baseColorFactor"] = { 0.125, 0.25, 0.5, 0.75 };
		pbr["metallicFactor"] = 0.375;
		pbr["roughnessFactor"] = 0.625;
		pbr["metallicRoughnessTexture"] = { { "index", 0 } };
		document["samplers"] = nlohmann::json::array({ {
			{ "magFilter", 9729 }, { "minFilter", 9729 },
			{ "wrapS", 33071 }, { "wrapT", 33071 },
		} });
		for (auto& texture : document["textures"])
			texture["sampler"] = 0;
	}, "normal_mapped_authored_tangents.gltf", false);
	ECS ecs;
	const auto model = ResourceLoader::load_model(ecs, resource.filename());
	const auto& renderable = model.meshes[0].renderables[0];
	const auto* pbr = find_pbr_material(renderable);
	ASSERT_NE(pbr, nullptr);
	EXPECT_TRUE(glm_equal(pbr->data.base_color_factor, glm::vec4(0.125f, 0.25f, 0.5f, 0.75f)));
	EXPECT_FLOAT_EQ(pbr->data.metallic_factor, 0.375f);
	EXPECT_FLOAT_EQ(pbr->data.roughness_factor, 0.625f);
	EXPECT_FLOAT_EQ(pbr->data.normal_scale, 0.5f);
	const auto* base_color = find_texture_material(renderable, ETextureSemantic::BASE_COLOR);
	const auto* metallic_roughness = find_texture_material(
		renderable, ETextureSemantic::METALLIC_ROUGHNESS);
	const auto* normal = find_texture_material(renderable, ETextureSemantic::NORMAL);
	ASSERT_NE(base_color, nullptr);
	ASSERT_NE(metallic_roughness, nullptr);
	ASSERT_NE(normal, nullptr);
	ASSERT_TRUE(pbr->textures.base_color.has_value());
	ASSERT_TRUE(pbr->textures.metallic_roughness.has_value());
	ASSERT_TRUE(pbr->textures.normal.has_value());
	EXPECT_EQ(pbr->textures.base_color->texture, base_color->get_id());
	EXPECT_EQ(pbr->textures.metallic_roughness->texture, metallic_roughness->get_id());
	EXPECT_EQ(pbr->textures.normal->texture, normal->get_id());
	const auto sampler = PbrMaterial::TextureSampler::clamp_to_edge(
		PbrMaterial::TextureSampler::MipmapMode::NONE);
	EXPECT_EQ(pbr->textures.base_color->sampler, sampler);
	EXPECT_EQ(pbr->textures.metallic_roughness->sampler, sampler);
	EXPECT_EQ(pbr->textures.normal->sampler, sampler);
}

TEST(ResourceLoaderTexturedPbr, imports_external_jpeg_uri)
{
	TemporaryExternalJpeg image;
	MutatedGltf resource([&](nlohmann::json& document)
	{
		document["images"][0] = { { "uri", image.filename() } };
	}, "static_mesh_textured.gltf", false);
	ECS ecs;
	const auto model = ResourceLoader::load_model(ecs, resource.filename());
	const auto* texture = find_texture_material(
		model.meshes[0].renderables[0], ETextureSemantic::BASE_COLOR);
	ASSERT_NE(texture, nullptr);
	EXPECT_GT(texture->width, 0u);
	EXPECT_GT(texture->height, 0u);
}

TEST(ResourceLoaderTexturedPbr, rejects_nonzero_texcoord_and_missing_texcoord_zero)
{
	MutatedGltf nonzero([](nlohmann::json& document)
	{
		document["materials"][0]["pbrMetallicRoughness"]["baseColorTexture"]["texCoord"] = 1;
	}, "static_mesh_textured.gltf", false);
	ECS ecs;
	EXPECT_THROW(ResourceLoader::load_model(ecs, nonzero.filename()), ResourceLoadError);

	MutatedGltf missing([](nlohmann::json& document)
	{
		document["meshes"][0]["primitives"][0]["attributes"].erase("TEXCOORD_0");
	}, "static_mesh_textured.gltf", false);
	EXPECT_THROW(ResourceLoader::load_model(ecs, missing.filename()), ResourceLoadError);
}

TEST(ResourceLoaderTexturedPbr, rejects_invalid_texture_image_references)
{
	using Mutation = std::function<void(nlohmann::json&)>;
	const std::vector<Mutation> cases{
		[](auto& document) {
			document["materials"][0]["pbrMetallicRoughness"]["baseColorTexture"]["index"] = 9; },
		[](auto& document) { document["textures"][0]["source"] = 9; },
	};
	for (const auto& mutate : cases)
	{
		MutatedGltf resource(mutate, "static_mesh_textured.gltf", false);
		ECS ecs;
		EXPECT_THROW(ResourceLoader::load_model(ecs, resource.filename()), ResourceLoadError);
		EXPECT_TRUE(ecs.get_material_system().take_retired().empty());
		EXPECT_TRUE(ecs.get_mesh_system().take_retired().empty());
	}
}

TEST(ResourceLoaderTexturedPbr, rejects_sampler_modes_outside_linear_repeat_or_clamp)
{
	using Mutation = std::function<void(nlohmann::json&)>;
	const std::vector<Mutation> cases{
		[](auto& document) { document["samplers"][0]["magFilter"] = 9728; }, // NEAREST
		[](auto& document) { document["samplers"][0]["minFilter"] = 9984; }, // NEAREST_MIPMAP_NEAREST
		[](auto& document) { document["samplers"][0]["minFilter"] = 9986; }, // NEAREST_MIPMAP_LINEAR
		[](auto& document) { document["samplers"][0]["wrapS"] = 33648; }, // MIRRORED_REPEAT
	};
	for (const auto& mutate : cases)
	{
		MutatedGltf resource([&](nlohmann::json& document)
		{
			document["samplers"] = nlohmann::json::array({ {
				{ "magFilter", 9729 }, { "minFilter", 9729 },
				{ "wrapS", 10497 }, { "wrapT", 10497 },
			} });
			document["textures"][0]["sampler"] = 0;
			mutate(document);
		}, "static_mesh_textured.gltf", false);
		ECS ecs;
		EXPECT_THROW(ResourceLoader::load_model(ecs, resource.filename()), ResourceLoadError);
	}
}

TEST(ResourceLoaderTexturedPbr, accepts_normal_map_without_base_color_map)
{
	ECS ecs;
	const auto model = ResourceLoader::load_model(ecs, "normal_map_without_base_color.gltf");
	const auto& renderable = model.meshes[0].renderables[0];
	EXPECT_EQ(renderable.pipeline_render_type, ERenderType::STANDARD);
	const auto* pbr = find_pbr_material(renderable);
	ASSERT_NE(pbr, nullptr);
	EXPECT_EQ(find_texture_material(renderable, ETextureSemantic::BASE_COLOR), nullptr);
	const auto* normal = find_texture_material(renderable, ETextureSemantic::NORMAL);
	ASSERT_NE(normal, nullptr);
	EXPECT_FALSE(pbr->textures.base_color.has_value());
	ASSERT_TRUE(pbr->textures.normal.has_value());
	EXPECT_EQ(pbr->textures.normal->texture, normal->get_id());
}

TEST(ResourceLoaderTexturedPbr, preserves_authored_tangents)
{
	ECS ecs;
	const auto model = ResourceLoader::load_model(ecs, "normal_mapped_authored_tangents.gltf");
	const auto& renderable = model.meshes[0].renderables[0];
	const auto& mesh = dynamic_cast<const TexMesh&>(
		ecs.get_mesh_system().get(renderable.mesh_owner->get_id()));
	for (const auto& vertex : mesh.get_vertices())
		EXPECT_TRUE(glm_equal(vertex.tangent, glm::vec4(-1.0f, 0.0f, 0.0f, -1.0f)));
}

TEST(ResourceLoaderTexturedPbr, generates_missing_mikktspace_tangents)
{
	ECS ecs;
	const auto model = ResourceLoader::load_model(ecs, "normal_mapped_missing_tangents.gltf");
	const auto& renderable = model.meshes[0].renderables[0];
	const auto& mesh = dynamic_cast<const TexMesh&>(
		ecs.get_mesh_system().get(renderable.mesh_owner->get_id()));
	for (const auto& vertex : mesh.get_vertices())
	{
		EXPECT_NEAR(glm::length(glm::vec3(vertex.tangent)), 1.0f, 0.001f);
		EXPECT_NEAR(glm::dot(glm::vec3(vertex.tangent), vertex.normal), 0.0f, 0.001f);
		EXPECT_TRUE(glm_equal(vertex.tangent, glm::vec4(-1.0f, 0.0f, 0.0f, -1.0f)));
	}
	ASSERT_EQ(model.warnings.size(), 1);
	EXPECT_NE(model.warnings.front().message.find("generated missing tangents"), std::string::npos);
}

TEST(ResourceLoaderTexturedPbr, rejects_invalid_or_required_but_missing_authored_tangents)
{
	ECS ecs;
	EXPECT_THROW(
		ResourceLoader::load_model(ecs, "normal_mapped_invalid_tangents.gltf"),
		ResourceLoadError);
	ResourceLoader::LoadOptions no_generation;
	no_generation.generate_missing_tangents = false;
	EXPECT_THROW(
		ResourceLoader::load_model(
			ecs, "normal_mapped_missing_tangents.gltf", no_generation),
		ResourceLoadError);
}

TEST(ResourceLoaderTexturedPbr, regenerates_invalid_authored_tangents_when_requested)
{
	ECS ecs;
	ResourceLoader::LoadOptions options;
	options.regenerate_invalid_tangents = true;
	const auto model = ResourceLoader::load_model(
		ecs, "normal_mapped_invalid_tangents.gltf", options);
	const auto& renderable = model.meshes[0].renderables[0];
	const auto& mesh = dynamic_cast<const TexMesh&>(
		ecs.get_mesh_system().get(renderable.mesh_owner->get_id()));
	for (const auto& vertex : mesh.get_vertices())
	{
		EXPECT_NEAR(glm::length(glm::vec3(vertex.tangent)), 1.0f, 0.001f);
		EXPECT_NEAR(glm::dot(glm::vec3(vertex.tangent), vertex.normal), 0.0f, 0.001f);
	}
	ASSERT_EQ(model.warnings.size(), 1);
	EXPECT_NE(model.warnings.front().message.find("regenerated invalid tangents"), std::string::npos);
	Object object;
	ecs.add_object(object);
	EXPECT_NO_THROW(ecs.add_renderable(
		model.meshes[0].renderables[0],
		object.get_id()));
}

TEST(ResourceLoaderTexturedPbr, imports_skinned_data_uri_png_and_tangents)
{
	ECS ecs;
	const auto model = ResourceLoader::load_model(ecs, "skinned_normal_mapped.gltf");
	ASSERT_EQ(model.meshes.size(), 1);
	ASSERT_TRUE(model.meshes[0].skeleton_id.has_value());
	const auto& renderable = model.meshes[0].renderables[0];
	EXPECT_EQ(renderable.pipeline_render_type, ERenderType::SKINNED);
	EXPECT_NE(find_texture_material(renderable, ETextureSemantic::BASE_COLOR), nullptr);
	EXPECT_NE(find_texture_material(renderable, ETextureSemantic::NORMAL), nullptr);
	const auto& mesh = dynamic_cast<const SkinnedMesh&>(
		ecs.get_mesh_system().get(renderable.mesh_owner->get_id()));
	ASSERT_EQ(mesh.get_vertices().size(), 3);
	for (const auto& vertex : mesh.get_vertices())
		EXPECT_TRUE(glm_equal(vertex.tangent, glm::vec4(-1.0f, 0.0f, 0.0f, -1.0f)));
}

TEST(ResourceLoaderTexturedPbr, generates_skinned_tangents_without_changing_skin_weights)
{
	ECS ecs;
	const auto model = ResourceLoader::load_model(
		ecs, "skinned_normal_mapped_missing_tangents.gltf");
	const auto& renderable = model.meshes[0].renderables[0];
	const auto& mesh = dynamic_cast<const SkinnedMesh&>(
		ecs.get_mesh_system().get(renderable.mesh_owner->get_id()));
	ASSERT_FALSE(mesh.get_vertices().empty());
	for (const auto& vertex : mesh.get_vertices())
	{
		EXPECT_NEAR(glm::length(glm::vec3(vertex.tangent)), 1.0f, 0.001f);
		EXPECT_EQ(vertex.bone_ids, glm::vec4(0.0f));
		EXPECT_EQ(vertex.bone_weights, glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
	}
}

TEST(ResourceLoaderTexturedPbr, shares_imported_material_resources_across_primitives)
{
	ECS ecs;
	const auto model = ResourceLoader::load_model(ecs, "normal_mapped_shared_material.gltf");
	ASSERT_EQ(model.meshes[0].renderables.size(), 2);
	const auto& first = model.meshes[0].renderables[0];
	const auto& second = model.meshes[0].renderables[1];
	ASSERT_EQ(first.material_owners.size(), second.material_owners.size());
	ASSERT_GE(first.material_owners.size(), 3);
	for (size_t index = 0; index < first.material_owners.size(); ++index)
		EXPECT_EQ(first.material_owners[index], second.material_owners[index]);
}

TEST(ResourceLoaderTexturedPbr, shares_semantically_compatible_texture_across_distinct_materials)
{
	MutatedGltf resource([](nlohmann::json& document)
	{
		auto second = document["materials"][0];
		second["pbrMetallicRoughness"]["roughnessFactor"] = 0.25;
		document["materials"].push_back(std::move(second));
		document["meshes"][0]["primitives"][1]["material"] = 1;
	}, "static_mesh_textured_shared_material.gltf", false);
	ECS ecs;
	const auto model = ResourceLoader::load_model(ecs, resource.filename());
	ASSERT_EQ(model.meshes[0].renderables.size(), 2);
	const auto& first = model.meshes[0].renderables[0];
	const auto& second = model.meshes[0].renderables[1];
	const auto* first_pbr = find_pbr_material(first);
	const auto* second_pbr = find_pbr_material(second);
	ASSERT_NE(first_pbr, nullptr);
	ASSERT_NE(second_pbr, nullptr);
	EXPECT_NE(first_pbr->get_id(), second_pbr->get_id());
	const auto* first_texture = find_texture_material(first, ETextureSemantic::BASE_COLOR);
	const auto* second_texture = find_texture_material(second, ETextureSemantic::BASE_COLOR);
	ASSERT_NE(first_texture, nullptr);
	ASSERT_NE(second_texture, nullptr);
	EXPECT_EQ(first_texture->get_id(), second_texture->get_id());
	ASSERT_TRUE(first_pbr->textures.base_color.has_value());
	ASSERT_TRUE(second_pbr->textures.base_color.has_value());
	EXPECT_EQ(first_pbr->textures.base_color->texture, first_texture->get_id());
	EXPECT_EQ(second_pbr->textures.base_color->texture, second_texture->get_id());
}

TEST(ResourceLoaderSkinnedColor, imports_pbr_material_without_texture_upload)
{
	const auto path = "skinned_color.gltf";
	const auto model = ResourceLoader::load_model(general_loader_ecs, path);
	ASSERT_EQ(model.meshes.size(), 1);
	ASSERT_EQ(model.meshes[0].renderables.size(), 1);
	const auto& renderable = model.meshes[0].renderables[0];
	EXPECT_EQ(renderable.pipeline_render_type, ERenderType::SKINNED_COLOR);
	EXPECT_TRUE(model.meshes[0].skeleton_id.has_value());
	ASSERT_EQ(renderable.material_owners.size(), 1);
	const auto* material = dynamic_cast<const PbrMaterial*>(
		&general_loader_ecs.get_material_system().get(renderable.material_owners[0]->get_id()));
	ASSERT_NE(material, nullptr);
	EXPECT_TRUE(glm_equal(material->data.base_color_factor, glm::vec4(0.25f, 0.5f, 0.75f, 1.0f)));
	EXPECT_FLOAT_EQ(material->data.metallic_factor, 0.0f);
	EXPECT_FLOAT_EQ(material->data.roughness_factor, 0.5f);
	EXPECT_NE(dynamic_cast<const SkinnedMesh*>(&general_loader_ecs.get_mesh_system().get(renderable.mesh_owner->get_id())), nullptr);
}

TEST(ResourceLoaderSkinning, rejects_more_than_four_bone_influences_per_vertex)
{
	const auto path = "skinned_too_many_influences.gltf";
	try
	{
		ResourceLoader::load_model(general_loader_ecs, path);
		FAIL() << "Expected ResourceLoadError";
	}
	catch (const ResourceLoadError& error)
	{
		EXPECT_NE(std::string(error.what()).find("maximum of 4 bone influences"), std::string::npos);
	}
}

TEST(ResourceLoaderStaticMesh, two_meshes_with_two_renderables_each)
{
	const auto model_path = "multi_mesh_multi_primitive.gltf";
	const auto model = ResourceLoader::load_model(general_loader_ecs, model_path);

	ASSERT_EQ(model.meshes.size(), 2);
	ASSERT_EQ(model.meshes[0].renderables.size(), 2);
	ASSERT_EQ(model.meshes[1].renderables.size(), 2);
	for (const auto& mesh : model.meshes)
	{
		for (const auto& renderable : mesh.renderables)
		{
			ASSERT_EQ(renderable.pipeline_render_type, ERenderType::COLOR);
			ASSERT_FALSE(mesh.skeleton_id.has_value());
			ASSERT_EQ(renderable.material_owners.size(), 1);
			const auto& material = general_loader_ecs.get_material_system().get(renderable.material_owners[0]->get_id());
			ASSERT_NE(dynamic_cast<const PbrMaterial*>(&material), nullptr);
		}
	}
}

TEST(ResourceLoaderVariants, scene_nodes_create_distinct_mesh_instances)
{
	const auto model = ResourceLoader::load_model(general_loader_ecs, "import_variants.gltf");

	ASSERT_EQ(model.meshes.size(), 2);
	EXPECT_EQ(model.meshes[0].name, "Right instance");
	EXPECT_EQ(model.meshes[1].name, "Left instance");
	ASSERT_EQ(model.meshes[0].renderables.size(), 1);
	ASSERT_EQ(model.meshes[1].renderables.size(), 1);
	EXPECT_EQ(model.meshes[0].renderables[0].name, "Right instance");
	EXPECT_EQ(model.meshes[1].renderables[0].name, "Left instance");
	EXPECT_TRUE(glm_equal(
		model.meshes[0].renderables[0].local_transform.get_pos(),
		glm::vec3(-3.0f, 0.0f, 0.0f)));
	EXPECT_TRUE(glm_equal(
		model.meshes[1].renderables[0].local_transform.get_pos(),
		glm::vec3(1.0f, 2.0f, 0.0f)));
}

TEST(ResourceLoaderVariants, explicit_scene_selection_uses_requested_scene)
{
	ResourceLoader::LoadOptions options;
	options.scene_index = 1;
	const auto model = ResourceLoader::load_model(general_loader_ecs, "import_variants.gltf", options);

	ASSERT_EQ(model.meshes.size(), 1);
	EXPECT_EQ(model.meshes[0].name, "Alternate scene mesh");
	EXPECT_EQ(model.meshes[0].source_node, 2);
	ASSERT_EQ(model.meshes[0].renderables.size(), 1);
	EXPECT_TRUE(glm_equal(
		model.meshes[0].renderables[0].local_transform.get_pos(),
		glm::vec3(0.0f, 4.0f, 0.0f)));
}

TEST(ResourceLoaderVariants, generates_normals_for_non_indexed_interleaved_triangle_strip)
{
	const auto model = ResourceLoader::load_model(general_loader_ecs, "import_variants.gltf");

	ASSERT_EQ(model.meshes[0].renderables.size(), 1);
	const auto& mesh = general_loader_ecs.get_mesh_system().get(model.meshes[0].renderables[0].mesh_owner->get_id());
	EXPECT_EQ(mesh.get_num_unique_vertices(), 4);
	EXPECT_EQ(mesh.get_indices(), (std::vector<uint32_t>{ 0, 2, 1, 2, 3, 1 }));
	ASSERT_EQ(model.warnings.size(), 4);
}

TEST(ResourceLoaderVariants, non_triangle_conversion_can_be_disabled)
{
	ResourceLoader::LoadOptions options;
	options.allow_non_triangle_primitives = false;
	EXPECT_THROW(
		ResourceLoader::load_model(general_loader_ecs, "import_variants.gltf", options),
		ResourceLoadError);
}

TEST(ResourceLoaderVariants, strict_mode_turns_import_warnings_into_errors)
{
	ResourceLoader::LoadOptions options;
	options.strict = true;
	EXPECT_THROW(
		ResourceLoader::load_model(general_loader_ecs, "import_variants.gltf", options),
		ResourceLoadError);
}
