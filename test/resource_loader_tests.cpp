#include "test_helper.hpp"

#include <resource_loader/resource_loader.hpp>
#include <utility.hpp>
#include <entity_component_system/ecs.hpp>
#include <entity_component_system/material_system.hpp>
#include <entity_component_system/mesh_system.hpp>
#include <renderable/material_factory.hpp>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <fstream>


class ResourceLoaderECS : public testing::Test
{
public:
	ResourceLoaderECS()
	{
		model = ResourceLoader::load_model(model_path);
	}

	glm::vec3 apply_transform(const glm::mat4& transform, const glm::vec3& v)
	{
		return glm::vec3(transform * glm::vec4(v, 1.0f));
	}

	const std::vector<Bone>& get_bones()
	{
		const auto skeleton_id = model.meshes[0].skeleton_id.value();
		const auto& skeleton = ECS::get().get_skeletal_component(skeleton_id);
		return skeleton.get_bones();
	}

	// extremely simple skinned model, contains a 2x2x4 (width,depth,height) cube mesh with 5 bones
	// the bones look a bit like this, they are perfectly axis aligned
	/*
		 |
		_|_
		 |
	*/
	const std::filesystem::path model_path = Utility::get_top_level_path()/"test/data/simple_test_model.gltf";
	ResourceLoader::LoadedModel model;
	glm::vec3 v1 = {0.0f, 0.0f, 0.0f};
	glm::vec3 v2 = {1.0f, 1.0f, 0.0f};
};

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
		path = std::filesystem::temp_directory_path()
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
};

class GeneratedGLBWithDDS
{
public:
	explicit GeneratedGLBWithDDS(const std::filesystem::path& dds_path)
	{
		static uint32_t sequence = 0;
		path = dds_path.parent_path() / fmt::format("krisp_test_dds_{}.glb", sequence++);

		std::ifstream template_file(
			Utility::get_top_level_path()/"test/data/static_mesh_textured.gltf");
		nlohmann::json document;
		template_file >> document;
		document["images"] = nlohmann::json::array({ {
			{ "uri", dds_path.filename().string() },
			{ "name", "generated_dds" },
		} });
		document["materials"][0]["normalTexture"] = { { "index", 0 } };

		std::string json = document.dump();
		while (json.size() % 4 != 0)
			json.push_back(' ');
		std::vector<unsigned char> bytes(20 + json.size(), 0);
		const auto write_u32 = [&bytes](const size_t offset, const uint32_t value)
		{
			for (size_t byte = 0; byte < 4; ++byte)
				bytes[offset + byte] = static_cast<unsigned char>(value >> (byte * 8));
		};
		write_u32(0, 0x46546C67); // "glTF"
		write_u32(4, 2);
		write_u32(8, static_cast<uint32_t>(bytes.size()));
		write_u32(12, static_cast<uint32_t>(json.size()));
		write_u32(16, 0x4E4F534A); // "JSON"
		std::copy(json.begin(), json.end(), bytes.begin() + 20);

		std::ofstream output(path, std::ios::binary);
		output.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
	}

	~GeneratedGLBWithDDS()
	{
		std::error_code error;
		std::filesystem::remove(path, error);
	}

	std::filesystem::path path;
};

class MutatedGltf
{
public:
	explicit MutatedGltf(const std::function<void(nlohmann::json&)>& mutate)
	{
		static uint32_t sequence = 0;
		path = std::filesystem::temp_directory_path()
			/ fmt::format("krisp_test_accessor_{}.gltf", sequence++);
		std::ifstream input(Utility::get_top_level_path()/"test/data/static_mesh_textured.gltf");
		nlohmann::json document;
		input >> document;
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
};
}

TEST(ResourceLoaderErrors, public_load_apis_report_typed_errors)
{
	const auto missing_model = Utility::get_top_level_path()/"test/data/does_not_exist.glb";
	const auto missing_texture = Utility::get_top_level_path()/"test/data/does_not_exist.png";

	EXPECT_THROW(ResourceLoader::load_model(missing_model), ResourceLoadError);
	EXPECT_THROW(ResourceLoader::fetch_texture(missing_texture), ResourceLoadError);
}

TEST(ResourceLoaderErrors, rejects_accessor_data_outside_its_buffer_view)
{
	MutatedGltf resource([](nlohmann::json& document)
	{
		document["bufferViews"][0]["byteLength"] = 4;
	});
	EXPECT_THROW(ResourceLoader::load_model(resource.path), ResourceLoadError);
}

TEST(ResourceLoaderErrors, rejects_accessor_stride_smaller_than_its_element)
{
	MutatedGltf resource([](nlohmann::json& document)
	{
		document["bufferViews"][0]["byteStride"] = 4;
	});
	EXPECT_THROW(ResourceLoader::load_model(resource.path), ResourceLoadError);
}

// test loading .gltf file with bones into std::vector<Bone>
TEST_F(ResourceLoaderECS, load_bones)
{
	ASSERT_EQ(model.meshes.size(), 1);
	ASSERT_EQ(model.meshes[0].renderables.size(), 1);
	const auto& renderable = model.meshes[0].renderables[0];
	ASSERT_EQ(renderable.pipeline_render_type, ERenderType::SKINNED);
	const auto skeleton_id = model.meshes[0].skeleton_id.value();
	const auto& skeleton = ECS::get().get_skeletal_component(skeleton_id);
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
	ASSERT_TRUE(glm_equal(get_bones()[3].relative_transform.get_orient(), Maths::zRot90));
	
	// left bone
	ASSERT_TRUE(glm_equal(get_bones()[4].relative_transform.get_pos(), Maths::up_vec));
	ASSERT_TRUE(glm_equal(get_bones()[4].relative_transform.get_scale(), Maths::identity_vec));
	ASSERT_TRUE(glm_equal(
		get_bones()[4].relative_transform.get_orient(), 
		glm::angleAxis(-Maths::PI/2.0f, Maths::forward_vec)));
}

TEST_F(ResourceLoaderECS, animations)
{
	ASSERT_EQ(model.animations.size(), 1);
	std::vector<BoneAnimation> bone_animations = ECS::get().get_skeletal_animations().at(model.animations[0]).bone_animations;
	ASSERT_EQ(bone_animations.size(), 5);
	ASSERT_EQ(
		bone_animations[0].translation_track.interpolation,
		BoneAnimation::Interpolation::STEP);
	for (const auto& bone_animation : bone_animations)
	{
		ASSERT_EQ(bone_animation.key_frames.size(), 4);
	}

	// check animation scale is never modified
	for (const auto& bone_animation : bone_animations)
	{
		for (const auto& key_frame : bone_animation.key_frames)
		{
			ASSERT_TRUE(glm_equal(key_frame.transform.get_scale(), Maths::identity_vec));
		}
	}

	// check pos is the same for all key frames
	const auto pos_checker = [](const BoneAnimation& animation, const glm::vec3& expected_pos)
	{
		for (const auto& key_frame : animation.key_frames)
		{
			if (!glm_equal(key_frame.transform.get_pos(), expected_pos))
			{
				return false;
			}
		}
		return true;
	};

	// check quat is the same for all key frames
	const auto quat_checker = [](const BoneAnimation& animation, const glm::quat& expected_quat)
	{
		for (const auto& key_frame : animation.key_frames)
		{
			if (!glm_equal(key_frame.transform.get_orient(), expected_quat))
			{
				return false;
			}
		}
		return true;
	};

	// root bone
	ASSERT_TRUE(pos_checker(bone_animations[0], Maths::zero_vec));
	ASSERT_TRUE(quat_checker(bone_animations[0], Maths::identity_quat));

	// mid bone
	ASSERT_TRUE(pos_checker(bone_animations[1], Maths::up_vec));
	ASSERT_TRUE(glm_equal(bone_animations[1].key_frames[0].transform.get_orient(), Maths::identity_quat));
	ASSERT_TRUE(glm_equal(bone_animations[1].key_frames[1].transform.get_orient(), 
		glm::angleAxis(Maths::PI/4.0f, Maths::forward_vec)));
	ASSERT_TRUE(glm_equal(bone_animations[1].key_frames[2].transform.get_orient(),
		glm::angleAxis(-Maths::PI/4.0f, Maths::forward_vec)));
	ASSERT_TRUE(glm_equal(bone_animations[1].key_frames[3].transform.get_orient(), Maths::identity_quat));
		
	// tip bone
	ASSERT_TRUE(pos_checker(bone_animations[2], Maths::up_vec));
	ASSERT_TRUE(glm_equal(bone_animations[2].key_frames[0].transform.get_orient(), Maths::identity_quat));
	ASSERT_TRUE(glm_equal(bone_animations[2].key_frames[1].transform.get_orient(), 
		glm::angleAxis(Maths::PI/8.0f, Maths::forward_vec)));
	ASSERT_TRUE(glm_equal(bone_animations[2].key_frames[2].transform.get_orient(), 
		glm::angleAxis(-Maths::PI/8.0f, Maths::forward_vec)));
	ASSERT_TRUE(glm_equal(bone_animations[2].key_frames[3].transform.get_orient(), Maths::identity_quat));

	// right bone
	ASSERT_TRUE(pos_checker(bone_animations[3], Maths::up_vec));
	ASSERT_TRUE(quat_checker(bone_animations[3], Maths::zRot90));
	
	// left bone
	ASSERT_TRUE(pos_checker(bone_animations[4], Maths::up_vec));
	ASSERT_TRUE(quat_checker(bone_animations[4], glm::angleAxis(-Maths::PI/2.0f, Maths::forward_vec)));
}

TEST_F(ResourceLoaderECS, standalone_animation_file_remaps_reordered_joints_by_name)
{
	const auto skeleton_id = model.meshes[0].skeleton_id.value();
	const auto path = Utility::get_top_level_path()/"test/data/standalone_animation.gltf";
	const size_t animations_before = ECS::get().get_skeletal_animations().size();
	const auto loaded = ResourceLoader::load_animations(path, skeleton_id);
	ASSERT_EQ(loaded.animations.size(), 2);
	EXPECT_EQ(ECS::get().get_skeletal_animations().size(), animations_before + 2);

	std::optional<AnimationID> root_move;
	std::optional<AnimationID> mid_turn;
	for (const auto id : loaded.animations)
	{
		const auto& animation = ECS::get().get_skeletal_animations().at(id);
		EXPECT_EQ(animation.source, "standalone_animation.gltf");
		EXPECT_TRUE(ECS::get().is_animation_compatible(skeleton_id, id));
		if (animation.name == "RootMove") root_move = id;
		if (animation.name == "MidTurn") mid_turn = id;
	}
	ASSERT_TRUE(root_move);
	ASSERT_TRUE(mid_turn);

	const auto& root_animation = ECS::get().get_skeletal_animations().at(*root_move);
	ASSERT_EQ(root_animation.bone_animations[0].translation_track.keys.size(), 2);
	EXPECT_TRUE(glm_equal(
		root_animation.bone_animations[0].translation_track.keys.back().value,
		glm::vec3(2.0f, 0.0f, 0.0f)));
	EXPECT_TRUE(root_animation.bone_animations[1].translation_track.keys.empty());

	const auto& mid_animation = ECS::get().get_skeletal_animations().at(*mid_turn);
	EXPECT_EQ(
		mid_animation.bone_animations[1].rotation_track.interpolation,
		BoneAnimation::Interpolation::STEP);
	EXPECT_TRUE(mid_animation.bone_animations[0].rotation_track.keys.empty());

	const auto reloaded = ResourceLoader::load_animations(path, skeleton_id);
	ASSERT_EQ(reloaded.animations.size(), 2);
	EXPECT_NE(reloaded.animations[0], loaded.animations[0]);
	EXPECT_EQ(ECS::get().get_skeletal_animations().size(), animations_before + 4);
}

TEST_F(ResourceLoaderECS, standalone_animation_rejects_incompatible_or_incomplete_rigs_atomically)
{
	const auto valid_skeleton = model.meshes[0].skeleton_id.value();
	const auto animation_path = Utility::get_top_level_path()/"test/data/standalone_animation.gltf";
	const size_t animations_before = ECS::get().get_skeletal_animations().size();

	auto incompatible_bones = get_bones();
	incompatible_bones[1].parent_node = Bone::NO_PARENT;
	const auto incompatible_skeleton = ECS::get().add_skeleton(incompatible_bones);
	EXPECT_THROW(ResourceLoader::load_animations(animation_path, incompatible_skeleton), ResourceLoadError);

	auto duplicate_bones = get_bones();
	duplicate_bones[1].name = duplicate_bones[0].name;
	const auto duplicate_skeleton = ECS::get().add_skeleton(duplicate_bones);
	EXPECT_THROW(ResourceLoader::load_animations(animation_path, duplicate_skeleton), ResourceLoadError);

	EXPECT_THROW(ResourceLoader::load_animations(
		Utility::get_top_level_path()/"test/data/standalone_animation_no_skin.gltf", valid_skeleton), ResourceLoadError);
	EXPECT_THROW(ResourceLoader::load_animations(
		Utility::get_top_level_path()/"test/data/standalone_animation_no_clips.gltf", valid_skeleton), ResourceLoadError);
	EXPECT_EQ(ECS::get().get_skeletal_animations().size(), animations_before);
}

TEST_F(ResourceLoaderECS, animation_playback_validates_rigs_and_replaces_active_clip)
{
	const auto skeleton_id = model.meshes[0].skeleton_id.value();
	const auto loaded = ResourceLoader::load_animations(
		Utility::get_top_level_path()/"test/data/standalone_animation.gltf", skeleton_id);
	ASSERT_EQ(loaded.animations.size(), 2);

	std::optional<AnimationID> root_move;
	std::optional<AnimationID> mid_turn;
	for (const auto id : loaded.animations)
	{
		const auto& animation = ECS::get().get_skeletal_animations().at(id);
		if (animation.name == "RootMove") root_move = id;
		if (animation.name == "MidTurn") mid_turn = id;
	}
	ASSERT_TRUE(root_move);
	ASSERT_TRUE(mid_turn);
	ECS::get().play_animation(skeleton_id, *root_move);
	ECS::get().process(0.5f);
	EXPECT_TRUE(glm_equal(
		ECS::get().get_skeletal_component(skeleton_id).get_bones()[0].relative_transform.get_pos(),
		glm::vec3(1.0f, 0.0f, 0.0f)));

	ECS::get().play_animation(skeleton_id, *mid_turn);
	EXPECT_TRUE(glm_equal(
		ECS::get().get_skeletal_component(skeleton_id).get_bones()[0].relative_transform.get_pos(),
		Maths::zero_vec));

	auto incompatible_bones = get_bones();
	incompatible_bones[0].name = "different-root";
	const auto incompatible_skeleton = ECS::get().add_skeleton(incompatible_bones);
	EXPECT_THROW(ECS::get().play_animation(incompatible_skeleton, *root_move), std::runtime_error);
	ECS::get().process(2.0f);
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

TEST(ResourceLoaderTextures, fetch_same_texture_path_twice_returns_same_material_id)
{
	const auto texture_path = Utility::get_texture("texture.jpg");

	const auto first = ResourceLoader::fetch_texture(texture_path);
	const auto second = ResourceLoader::fetch_texture(texture_path);

	ASSERT_EQ(first, second);
	EXPECT_GE(MaterialSystem::get_num_owners(first), 2);
}

TEST(ResourceLoaderTextures, caches_texture_variants_by_semantic_and_recovers_stale_entries)
{
	constexpr uint32_t DXT5 = 0x35545844;
	GeneratedDDS dds(4, 4, 1, DXT5);
	const auto& texture_path = dds.path;
	const auto base_color = ResourceLoader::fetch_texture(
		texture_path, ETextureSemantic::BASE_COLOR);
	const auto normal = ResourceLoader::fetch_texture(
		texture_path, ETextureSemantic::NORMAL);
	EXPECT_NE(base_color, normal);
	EXPECT_EQ(dynamic_cast<const TextureMaterial&>(MaterialSystem::get(base_color)).semantic,
		ETextureSemantic::BASE_COLOR);
	EXPECT_EQ(dynamic_cast<const TextureMaterial&>(MaterialSystem::get(normal)).semantic,
		ETextureSemantic::NORMAL);

	EXPECT_EQ(MaterialSystem::unregister_owner(base_color), 0);
	EXPECT_FALSE(MaterialSystem::contains(base_color));
	const auto reloaded = ResourceLoader::fetch_texture(
		texture_path, ETextureSemantic::BASE_COLOR);
	EXPECT_NE(reloaded, base_color);
	EXPECT_EQ(MaterialSystem::get_num_owners(reloaded), 1);
}

TEST(ResourceLoaderTextures, loads_generated_bc3_dds_with_complete_mip_chain)
{
	constexpr uint32_t DXT5 = 0x35545844;
	GeneratedDDS dds(8, 8, 4, DXT5);
	const auto material_id = ResourceLoader::fetch_texture(dds.path);
	const auto& material = static_cast<const TextureMaterial&>(MaterialSystem::get(material_id));

	EXPECT_EQ(material.width, 8);
	EXPECT_EQ(material.height, 8);
	EXPECT_EQ(material.channels, 4);
	EXPECT_EQ(material.format, ETextureFormat::BC3);
	EXPECT_EQ(material.data_len, 112);
	EXPECT_EQ(material.mip_sizes, (std::vector<size_t>{ 64, 16, 16, 16 }));
	ASSERT_NE(material.data, nullptr);
	EXPECT_EQ(std::to_integer<unsigned char>(*material.data->get()), 0x7f);
}

TEST(ResourceLoaderTextures, rejects_unsupported_or_truncated_dds_files)
{
	constexpr uint32_t DXT1 = 0x31545844;
	constexpr uint32_t DXT5 = 0x35545844;
	GeneratedDDS unsupported(4, 4, 1, DXT1);
	GeneratedDDS truncated(8, 8, 4, DXT5, 111);

	EXPECT_THROW(ResourceLoader::fetch_texture(unsupported.path), ResourceLoadError);
	EXPECT_THROW(ResourceLoader::fetch_texture(truncated.path), ResourceLoadError);
}

TEST(ResourceLoaderTextures, loads_external_dds_references_from_generated_glb)
{
	constexpr uint32_t DXT5 = 0x35545844;
	GeneratedDDS dds(8, 8, 4, DXT5);
	GeneratedGLBWithDDS glb(dds.path);
	ResourceLoader::LoadOptions options;
	options.generate_missing_tangents = true;
	const auto model = ResourceLoader::load_model(glb.path, options);

	ASSERT_EQ(model.meshes.size(), 1);
	ASSERT_EQ(model.meshes[0].renderables.size(), 1);
	const auto& material_ids = model.meshes[0].renderables[0].material_ids;
	ASSERT_EQ(material_ids.size(), 2);
	const auto& base_color = dynamic_cast<const TextureMaterial&>(
		MaterialSystem::get(material_ids[0]));
	const auto& normal = dynamic_cast<const TextureMaterial&>(
		MaterialSystem::get(material_ids[1]));
	EXPECT_EQ(base_color.format, ETextureFormat::BC3);
	EXPECT_EQ(base_color.semantic, ETextureSemantic::BASE_COLOR);
	EXPECT_EQ(base_color.mip_sizes, (std::vector<size_t>{ 64, 16, 16, 16 }));
	EXPECT_EQ(normal.format, ETextureFormat::BC3);
	EXPECT_EQ(normal.semantic, ETextureSemantic::NORMAL);
	EXPECT_EQ(normal.mip_sizes, base_color.mip_sizes);
}

TEST(ResourceLoaderStaticMesh, single_mesh_with_texture)
{
	const auto model_path = Utility::get_top_level_path()/"test/data/static_mesh_textured.gltf";
	const auto model = ResourceLoader::load_model(model_path);

	ASSERT_EQ(model.meshes.size(), 1);
	ASSERT_EQ(model.meshes[0].renderables.size(), 1);

	const auto& renderable = model.meshes[0].renderables[0];
	ASSERT_EQ(renderable.pipeline_render_type, ERenderType::STANDARD);
	ASSERT_FALSE(model.meshes[0].skeleton_id.has_value());

	ASSERT_EQ(renderable.material_ids.size(), 1);
	const auto& material = MaterialSystem::get(renderable.material_ids[0]);
	const auto* tex_material = dynamic_cast<const TextureMaterial*>(&material);
	ASSERT_NE(tex_material, nullptr);
	ASSERT_EQ(tex_material->width, 2);
	ASSERT_EQ(tex_material->height, 2);
}

TEST(ResourceLoaderSpecularMaps, imports_khr_materials_specular_maps)
{
	MutatedGltf resource([](nlohmann::json& document)
	{
		document["materials"][0]["extensions"]["KHR_materials_specular"] = {
			{ "specularFactor", 0.35 },
			{ "specularColorFactor", { 0.25, 0.5, 0.75 } },
			{ "specularTexture", { { "index", 0 } } },
			{ "specularColorTexture", { { "index", 0 } } },
		};
	});
	const auto model = ResourceLoader::load_model(resource.path);
	const auto& renderable = model.meshes[0].renderables[0];
	const TexturedMatGroup group(renderable.material_ids);

	ASSERT_TRUE(group.specular_strength_mat);
	ASSERT_TRUE(group.specular_color_mat);
	EXPECT_EQ(dynamic_cast<const TextureMaterial&>(
		MaterialSystem::get(*group.specular_strength_mat)).semantic,
		ETextureSemantic::SPECULAR_STRENGTH);
	EXPECT_EQ(dynamic_cast<const TextureMaterial&>(
		MaterialSystem::get(*group.specular_color_mat)).semantic,
		ETextureSemantic::SPECULAR_COLOR);
}

TEST(ResourceLoaderSpecularMaps, rejects_nonzero_specular_texcoord)
{
	MutatedGltf resource([](nlohmann::json& document)
	{
		document["materials"][0]["extensions"]["KHR_materials_specular"] = {
			{ "specularTexture", { { "index", 0 }, { "texCoord", 1 } } },
		};
	});
	EXPECT_THROW(ResourceLoader::load_model(resource.path), ResourceLoadError);
}

TEST(ResourceLoaderSpecularMaps, imports_specular_map_without_base_color_texture)
{
	MutatedGltf resource([](nlohmann::json& document)
	{
		document["materials"][0]["pbrMetallicRoughness"].erase("baseColorTexture");
		document["materials"][0]["extensions"]["KHR_materials_specular"] = {
			{ "specularTexture", { { "index", 0 } } },
		};
	});
	const auto model = ResourceLoader::load_model(resource.path);
	const auto& renderable = model.meshes[0].renderables[0];

	EXPECT_EQ(renderable.pipeline_render_type, ERenderType::STANDARD);
	const TexturedMatGroup group(renderable.material_ids);
	EXPECT_TRUE(group.specular_strength_mat);
	EXPECT_EQ(group.base_color_mat, MaterialFactory::fetch_white_texture());
}

TEST(ResourceLoaderStaticMesh, shared_textured_material_across_primitives_reuses_cached_material)
{
	const auto model_path = Utility::get_top_level_path()/"test/data/static_mesh_textured_shared_material.gltf";
	const auto model = ResourceLoader::load_model(model_path);

	ASSERT_EQ(model.meshes.size(), 1);
	ASSERT_EQ(model.meshes[0].renderables.size(), 2);

	const auto first_mat_id = model.meshes[0].renderables[0].material_ids[0];
	const auto second_mat_id = model.meshes[0].renderables[1].material_ids[0];
	ASSERT_EQ(first_mat_id, second_mat_id);
	EXPECT_EQ(MaterialSystem::get_num_owners(first_mat_id), 2);

	const auto& material = MaterialSystem::get(first_mat_id);
	const auto* tex_material = dynamic_cast<const TextureMaterial*>(&material);
	ASSERT_NE(tex_material, nullptr);
	ASSERT_EQ(tex_material->width, 2);
	ASSERT_EQ(tex_material->height, 2);
}

TEST(ResourceLoaderNormalMaps, missing_tangents_fail_by_default)
{
	const auto path = Utility::get_top_level_path()/"test/data/normal_mapped_missing_tangents.gltf";
	const ResourceLoader::LoadOptions options;
	EXPECT_THROW(ResourceLoader::load_model(path, options), ResourceLoadError);
}

TEST(ResourceLoaderNormalMaps, missing_tangents_can_be_generated)
{
	ResourceLoader::LoadOptions options;
	options.generate_missing_tangents = true;
	const auto path = Utility::get_top_level_path()/"test/data/normal_mapped_missing_tangents.gltf";
	const auto model = ResourceLoader::load_model(path, options);

	ASSERT_EQ(model.meshes.size(), 1);
	ASSERT_EQ(model.meshes[0].renderables.size(), 1);
	const auto& renderable = model.meshes[0].renderables[0];
	ASSERT_EQ(renderable.material_ids.size(), 2);
	const auto& base_material = dynamic_cast<const TextureMaterial&>(
		MaterialSystem::get(renderable.material_ids[0]));
	const auto& normal_material = dynamic_cast<const TextureMaterial&>(
		MaterialSystem::get(renderable.material_ids[1]));
	EXPECT_EQ(base_material.semantic, ETextureSemantic::BASE_COLOR);
	EXPECT_EQ(normal_material.semantic, ETextureSemantic::NORMAL);

	const auto& mesh = dynamic_cast<const TexMesh&>(MeshSystem::get(renderable.mesh_id));
	ASSERT_FALSE(mesh.get_vertices().empty());
	for (const auto& vertex : mesh.get_vertices())
	{
		EXPECT_NEAR(glm::length(glm::vec3(vertex.tangent)), 1.0f, 0.001f);
		EXPECT_NEAR(glm::dot(glm::vec3(vertex.tangent), vertex.normal), 0.0f, 0.001f);
		EXPECT_TRUE(vertex.tangent.w == 1.0f || vertex.tangent.w == -1.0f);
	}
	ASSERT_EQ(model.warnings.size(), 1);
	EXPECT_NE(model.warnings[0].message.find("generated missing tangents"), std::string::npos);
}

TEST(ResourceLoaderNormalMaps, invalid_tangents_fail_when_generation_is_disabled)
{
	const auto path = Utility::get_top_level_path()/"test/data/normal_mapped_invalid_tangents.gltf";
	const ResourceLoader::LoadOptions options;
	EXPECT_THROW(ResourceLoader::load_model(path, options), ResourceLoadError);
}

TEST(ResourceLoaderNormalMaps, invalid_tangents_are_regenerated)
{
	const auto path = Utility::get_top_level_path()/"test/data/normal_mapped_invalid_tangents.gltf";
	const auto model = ResourceLoader::load_model(path);
	const auto& renderable = model.meshes[0].renderables[0];
	const auto& mesh = dynamic_cast<const TexMesh&>(MeshSystem::get(renderable.mesh_id));
	for (const auto& vertex : mesh.get_vertices())
	{
		EXPECT_NEAR(glm::length(glm::vec3(vertex.tangent)), 1.0f, 0.001f);
		EXPECT_NEAR(glm::dot(glm::vec3(vertex.tangent), vertex.normal), 0.0f, 0.001f);
		EXPECT_TRUE(vertex.tangent.w == 1.0f || vertex.tangent.w == -1.0f);
	}
	ASSERT_EQ(model.warnings.size(), 1);
	EXPECT_NE(model.warnings[0].message.find("regenerated invalid tangents"), std::string::npos);
}

TEST(ResourceLoaderNormalMaps, authored_tangents_are_preserved_and_scale_warns)
{
	const auto path = Utility::get_top_level_path()/"test/data/normal_mapped_authored_tangents.gltf";
	const auto model = ResourceLoader::load_model(path);
	const auto& renderable = model.meshes[0].renderables[0];
	const auto& mesh = dynamic_cast<const TexMesh&>(MeshSystem::get(renderable.mesh_id));
	for (const auto& vertex : mesh.get_vertices())
		EXPECT_TRUE(glm_equal(vertex.tangent, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f)));
	ASSERT_EQ(model.warnings.size(), 1);
	EXPECT_NE(model.warnings[0].message.find("normalTexture.scale"), std::string::npos);

	ResourceLoader::LoadOptions strict_options;
	strict_options.strict = true;
	EXPECT_THROW(ResourceLoader::load_model(path, strict_options), ResourceLoadError);
}

TEST(ResourceLoaderNormalMaps, shared_material_registers_all_texture_owners)
{
	const auto path = Utility::get_top_level_path()/"test/data/normal_mapped_shared_material.gltf";
	const auto model = ResourceLoader::load_model(path);
	ASSERT_EQ(model.meshes[0].renderables.size(), 2);
	const auto& first_ids = model.meshes[0].renderables[0].material_ids;
	const auto& second_ids = model.meshes[0].renderables[1].material_ids;
	ASSERT_EQ(first_ids, second_ids);
	ASSERT_EQ(first_ids.size(), 2);
	EXPECT_EQ(MaterialSystem::get_num_owners(first_ids[0]), 2);
	EXPECT_EQ(MaterialSystem::get_num_owners(first_ids[1]), 2);
}

TEST(ResourceLoaderNormalMaps, normal_map_without_base_color_is_rejected)
{
	const auto path = Utility::get_top_level_path()/"test/data/normal_map_without_base_color.gltf";
	EXPECT_THROW(ResourceLoader::load_model(path), ResourceLoadError);
}

TEST(ResourceLoaderNormalMaps, skinned_mesh_imports_normal_map_and_tangents)
{
	const auto path = Utility::get_top_level_path()/"test/data/skinned_normal_mapped.gltf";
	const auto model = ResourceLoader::load_model(path);
	ASSERT_EQ(model.meshes.size(), 1);
	ASSERT_EQ(model.meshes[0].renderables.size(), 1);
	const auto& renderable = model.meshes[0].renderables[0];
	EXPECT_EQ(renderable.pipeline_render_type, ERenderType::SKINNED);
	ASSERT_TRUE(model.meshes[0].skeleton_id.has_value());
	ASSERT_EQ(renderable.material_ids.size(), 2);
	const auto& mesh = dynamic_cast<const SkinnedMesh&>(MeshSystem::get(renderable.mesh_id));
	ASSERT_EQ(mesh.get_vertices().size(), 3);
	for (const auto& vertex : mesh.get_vertices())
		EXPECT_TRUE(glm_equal(vertex.tangent, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f)));
}

TEST(ResourceLoaderSkinnedColor, imports_color_material_without_texture_upload)
{
	const auto path = Utility::get_top_level_path()/"test/data/skinned_color.gltf";
	const auto model = ResourceLoader::load_model(path);
	ASSERT_EQ(model.meshes.size(), 1);
	ASSERT_EQ(model.meshes[0].renderables.size(), 1);
	const auto& renderable = model.meshes[0].renderables[0];
	EXPECT_EQ(renderable.pipeline_render_type, ERenderType::SKINNED_COLOR);
	EXPECT_TRUE(model.meshes[0].skeleton_id.has_value());
	ASSERT_EQ(renderable.material_ids.size(), 1);
	const auto* material = dynamic_cast<const ColorMaterial*>(&MaterialSystem::get(renderable.material_ids[0]));
	ASSERT_NE(material, nullptr);
	EXPECT_TRUE(glm_equal(material->data.diffuse, glm::vec3(0.25f, 0.5f, 0.75f)));
	EXPECT_NE(dynamic_cast<const SkinnedMesh*>(&MeshSystem::get(renderable.mesh_id)), nullptr);
}

TEST(ResourceLoaderSkinning, rejects_more_than_four_bone_influences_per_vertex)
{
	const auto path = Utility::get_top_level_path()/"test/data/skinned_too_many_influences.gltf";
	try
	{
		ResourceLoader::load_model(path);
		FAIL() << "Expected ResourceLoadError";
	}
	catch (const ResourceLoadError& error)
	{
		EXPECT_NE(std::string(error.what()).find("maximum of 4 bone influences"), std::string::npos);
	}
}

TEST(ResourceLoaderNormalMaps, skinned_mesh_can_generate_missing_tangents)
{
	ResourceLoader::LoadOptions options;
	options.generate_missing_tangents = true;
	const auto path = Utility::get_top_level_path()/"test/data/skinned_normal_mapped_missing_tangents.gltf";
	const auto model = ResourceLoader::load_model(path, options);
	const auto& renderable = model.meshes[0].renderables[0];
	const auto& mesh = dynamic_cast<const SkinnedMesh&>(MeshSystem::get(renderable.mesh_id));
	ASSERT_FALSE(mesh.get_vertices().empty());
	for (const auto& vertex : mesh.get_vertices())
	{
		EXPECT_NEAR(glm::length(glm::vec3(vertex.tangent)), 1.0f, 0.001f);
		EXPECT_EQ(vertex.bone_ids, glm::vec4(0.0f));
		EXPECT_EQ(vertex.bone_weights, glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
	}
}

TEST(ResourceLoaderStaticMesh, two_meshes_with_two_renderables_each)
{
	const auto model_path = Utility::get_top_level_path()/"test/data/multi_mesh_multi_primitive.gltf";
	const auto model = ResourceLoader::load_model(model_path);

	ASSERT_EQ(model.meshes.size(), 2);
	ASSERT_EQ(model.meshes[0].renderables.size(), 2);
	ASSERT_EQ(model.meshes[1].renderables.size(), 2);
	for (const auto& mesh : model.meshes)
	{
		for (const auto& renderable : mesh.renderables)
		{
			ASSERT_EQ(renderable.pipeline_render_type, ERenderType::COLOR);
			ASSERT_FALSE(mesh.skeleton_id.has_value());
			ASSERT_EQ(renderable.material_ids.size(), 1);
			const auto& material = MaterialSystem::get(renderable.material_ids[0]);
			ASSERT_NE(dynamic_cast<const ColorMaterial*>(&material), nullptr);
		}
	}
}

TEST(ResourceLoaderVariants, scene_nodes_create_distinct_mesh_instances)
{
	const auto model = ResourceLoader::load_model(Utility::get_top_level_path()/"test/data/import_variants.gltf");

	ASSERT_EQ(model.meshes.size(), 2);
	EXPECT_EQ(model.meshes[0].name, "Right instance");
	EXPECT_EQ(model.meshes[1].name, "Left instance");
	EXPECT_TRUE(glm_equal(model.meshes[0].transform.get_pos(), glm::vec3(3.0f, 0.0f, 0.0f)));
	EXPECT_TRUE(glm_equal(model.meshes[1].transform.get_pos(), glm::vec3(-1.0f, 2.0f, 0.0f)));
}

TEST(ResourceLoaderVariants, explicit_scene_selection_uses_requested_scene)
{
	ResourceLoader::LoadOptions options;
	options.scene_index = 1;
	const auto model = ResourceLoader::load_model(Utility::get_top_level_path()/"test/data/import_variants.gltf", options);

	ASSERT_EQ(model.meshes.size(), 1);
	EXPECT_EQ(model.meshes[0].name, "Alternate scene mesh");
	EXPECT_EQ(model.meshes[0].source_node, 2);
	EXPECT_TRUE(glm_equal(model.meshes[0].transform.get_pos(), glm::vec3(0.0f, 4.0f, 0.0f)));
}

TEST(ResourceLoaderVariants, generates_normals_for_non_indexed_interleaved_triangle_strip)
{
	const auto model = ResourceLoader::load_model(Utility::get_top_level_path()/"test/data/import_variants.gltf");

	ASSERT_EQ(model.meshes[0].renderables.size(), 1);
	const auto& mesh = MeshSystem::get(model.meshes[0].renderables[0].mesh_id);
	EXPECT_EQ(mesh.get_num_unique_vertices(), 4);
	EXPECT_EQ(mesh.get_indices(), (std::vector<uint32_t>{ 0, 1, 2, 2, 1, 3 }));
	ASSERT_EQ(model.warnings.size(), 4);
}

TEST(ResourceLoaderVariants, non_triangle_conversion_can_be_disabled)
{
	ResourceLoader::LoadOptions options;
	options.allow_non_triangle_primitives = false;
	EXPECT_THROW(
		ResourceLoader::load_model(Utility::get_top_level_path()/"test/data/import_variants.gltf", options),
		ResourceLoadError);
}

TEST(ResourceLoaderVariants, strict_mode_turns_import_warnings_into_errors)
{
	ResourceLoader::LoadOptions options;
	options.strict = true;
	EXPECT_THROW(
		ResourceLoader::load_model(Utility::get_top_level_path()/"test/data/import_variants.gltf", options),
		ResourceLoadError);
}
