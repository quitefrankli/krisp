#include "test_helper.hpp"

#include <resource_loader/resource_loader.hpp>
#include <utility.hpp>
#include <entity_component_system/ecs.hpp>
#include <entity_component_system/material_system.hpp>
#include <entity_component_system/mesh_system.hpp>

#include <gtest/gtest.h>


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
		const auto& renderable = model.meshes[0].renderables[0];
		const auto skeleton_id = renderable.skeleton_id.value();
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


// test loading .gltf file with bones into std::vector<Bone>
TEST_F(ResourceLoaderECS, load_bones)
{
	ASSERT_EQ(model.meshes.size(), 1);
	ASSERT_EQ(model.meshes[0].renderables.size(), 1);
	const auto& renderable = model.meshes[0].renderables[0];
	ASSERT_EQ(renderable.pipeline_render_type, ERenderType::SKINNED);
	const auto skeleton_id = renderable.skeleton_id.value();
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

TEST(ResourceLoaderTextures, fetch_same_texture_path_twice_returns_same_material_id)
{
	const auto texture_path = Utility::get_texture("texture.jpg");

	const auto first = ResourceLoader::fetch_texture(texture_path);
	const auto second = ResourceLoader::fetch_texture(texture_path);

	ASSERT_EQ(first, second);
}

TEST(ResourceLoaderStaticMesh, single_mesh_with_texture)
{
	const auto model_path = Utility::get_top_level_path()/"test/data/static_mesh_textured.gltf";
	const auto model = ResourceLoader::load_model(model_path);

	ASSERT_EQ(model.meshes.size(), 1);
	ASSERT_EQ(model.meshes[0].renderables.size(), 1);

	const auto& renderable = model.meshes[0].renderables[0];
	ASSERT_EQ(renderable.pipeline_render_type, ERenderType::STANDARD);
	ASSERT_FALSE(renderable.skeleton_id.has_value());

	ASSERT_EQ(renderable.material_ids.size(), 1);
	const auto& material = MaterialSystem::get(renderable.material_ids[0]);
	const auto* tex_material = dynamic_cast<const TextureMaterial*>(&material);
	ASSERT_NE(tex_material, nullptr);
	ASSERT_EQ(tex_material->width, 2);
	ASSERT_EQ(tex_material->height, 2);
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
	EXPECT_THROW(ResourceLoader::load_model(path), std::runtime_error);
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
	EXPECT_THROW(ResourceLoader::load_model(path, strict_options), std::runtime_error);
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
	EXPECT_THROW(ResourceLoader::load_model(path), std::runtime_error);
}

TEST(ResourceLoaderNormalMaps, skinned_mesh_imports_normal_map_and_tangents)
{
	const auto path = Utility::get_top_level_path()/"test/data/skinned_normal_mapped.gltf";
	const auto model = ResourceLoader::load_model(path);
	ASSERT_EQ(model.meshes.size(), 1);
	ASSERT_EQ(model.meshes[0].renderables.size(), 1);
	const auto& renderable = model.meshes[0].renderables[0];
	EXPECT_EQ(renderable.pipeline_render_type, ERenderType::SKINNED);
	ASSERT_TRUE(renderable.skeleton_id.has_value());
	ASSERT_EQ(renderable.material_ids.size(), 2);
	const auto& mesh = dynamic_cast<const SkinnedMesh&>(MeshSystem::get(renderable.mesh_id));
	ASSERT_EQ(mesh.get_vertices().size(), 3);
	for (const auto& vertex : mesh.get_vertices())
		EXPECT_TRUE(glm_equal(vertex.tangent, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f)));
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
			ASSERT_FALSE(renderable.skeleton_id.has_value());
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
		std::runtime_error);
}

TEST(ResourceLoaderVariants, strict_mode_turns_import_warnings_into_errors)
{
	ResourceLoader::LoadOptions options;
	options.strict = true;
	EXPECT_THROW(
		ResourceLoader::load_model(Utility::get_top_level_path()/"test/data/import_variants.gltf", options),
		std::runtime_error);
}
