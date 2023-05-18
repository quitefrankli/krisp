#include "test_helper.hpp"

#include <resource_loader.hpp>
#include <utility.hpp>
#include <shapes/shape.hpp>

#include <gtest/gtest.h>


class ResourceLoaderECS : public testing::Test
{
public:
	ResourceLoaderECS()
	{
		model = ResourceLoader::get().load_model(model_path.string());
	}

	glm::vec3 apply_transform(const glm::mat4& transform, const glm::vec3& v)
	{
		return glm::vec3(transform * glm::vec4(v, 1.0f));
	}

	// extremely simple skinned model, contains a 2x2x4 (width,depth,height) cube mesh with 5 bones
	// the bones look a bit like this, they are perfectly axis aligned
	/*
		 |
		_|_
		 |
	*/
	const std::filesystem::path model_path = Utility::get().get_top_level_path()/"test/simple_test_model.gltf";
	ResourceLoader::LoadedModel model;
	glm::vec3 v1 = {0.0f, 0.0f, 0.0f};
	glm::vec3 v2 = {1.0f, 1.0f, 0.0f};
};


// test loading .gltf file with bones into std::vector<Bone>
TEST_F(ResourceLoaderECS, load_bones)
{
	ASSERT_EQ(model.shapes.size(), 1);
	ASSERT_EQ(model.bones.size(), 5);
}

TEST_F(ResourceLoaderECS, bone_relative_transforms)
{
	// root bone
	ASSERT_TRUE(glm_equal(model.bones[0].relative_transform.get_mat4(), Maths::identity_mat));

	// mid bone
	ASSERT_TRUE(glm_equal(
		model.bones[1].relative_transform.get_mat4(), 
		glm::translate(Maths::identity_mat, Maths::up_vec)));
		
	// tip bone
	ASSERT_TRUE(glm_equal(
		model.bones[2].relative_transform.get_mat4(), 
		glm::translate(Maths::identity_mat, Maths::up_vec)));

	// right bone
	ASSERT_TRUE(glm_equal(model.bones[3].relative_transform.get_pos(), Maths::up_vec));
	ASSERT_TRUE(glm_equal(model.bones[3].relative_transform.get_scale(), Maths::identity_vec));
	ASSERT_TRUE(glm_equal(model.bones[3].relative_transform.get_orient(), Maths::zRot90));
	
	// left bone
	ASSERT_TRUE(glm_equal(model.bones[4].relative_transform.get_pos(), Maths::up_vec));
	ASSERT_TRUE(glm_equal(model.bones[4].relative_transform.get_scale(), Maths::identity_vec));
	ASSERT_TRUE(glm_equal(
		model.bones[4].relative_transform.get_orient(), 
		glm::angleAxis(-Maths::PI/2.0f, Maths::forward_vec)));
}

TEST_F(ResourceLoaderECS, animations)
{
	ASSERT_EQ(model.animations.size(), 1);
	auto& bone_animations = model.animations.begin()->second;
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