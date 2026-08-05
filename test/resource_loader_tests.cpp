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
		std::vector<unsigned char> bytes(128, 0);
		const auto write_u32 = [&bytes](const size_t offset, const uint32_t value)
		{
			for (size_t byte = 0; byte < 4; ++byte)
				bytes[offset + byte] = static_cast<unsigned char>(value >> (byte * 8));
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
		bytes.resize(128 + payload_size.value_or(expected_payload), 0x7f);
		std::ofstream output(path, std::ios::binary);
		output.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
	}

	~GeneratedDDS()
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
		std::string_view template_filename = "static_mesh_textured.gltf")
	{
		static uint32_t sequence = 0;
		path = Utility::get_top_level_path()/"test/data"
			/ fmt::format("krisp_test_accessor_{}.gltf", sequence++);
		std::ifstream input(Utility::get_model(template_filename));
		nlohmann::json document;
		input >> document;
		if (template_filename == "static_mesh_textured.gltf")
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
}

TEST(ResourceLoaderMaterials, rejects_unsupported_material_features_with_material_context)
{
	using Mutation = std::function<void(nlohmann::json&)>;
	const std::vector<std::pair<std::string, Mutation>> cases{
		{ "baseColorTexture", [](auto& material) {
			material["pbrMetallicRoughness"]["baseColorTexture"] = { { "index", 0 } }; } },
		{ "metallicRoughnessTexture", [](auto& material) {
			material["pbrMetallicRoughness"]["metallicRoughnessTexture"] = { { "index", 0 } }; } },
		{ "normalTexture", [](auto& material) { material["normalTexture"] = { { "index", 0 } }; } },
		{ "occlusionTexture", [](auto& material) { material["occlusionTexture"] = { { "index", 0 } }; } },
		{ "emissiveTexture", [](auto& material) { material["emissiveTexture"] = { { "index", 0 } }; } },
		{ "emissiveFactor", [](auto& material) { material["emissiveFactor"] = { 0.0, 0.1, 0.0 }; } },
		{ "alphaMode", [](auto& material) { material["alphaMode"] = "MASK"; } },
		{ "doubleSided", [](auto& material) { material["doubleSided"] = true; } },
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

TEST(ResourceLoaderMaterials, rejects_unsupported_unused_material_before_importing_asset)
{
	MutatedGltf resource([](nlohmann::json& document)
	{
		document["materials"].push_back({
			{ "name", "Unused textured material" },
			{ "pbrMetallicRoughness", {
				{ "baseColorTexture", { { "index", 0 } } },
			} },
		});
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
		EXPECT_NE(message.find("Unused textured material"), std::string::npos);
		EXPECT_NE(message.find("baseColorTexture"), std::string::npos);
	}
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
	ASSERT_EQ(model.warnings.size(), 1);
	EXPECT_NE(model.warnings.front().message.find("ignored"), std::string::npos);

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
