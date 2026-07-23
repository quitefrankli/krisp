#include <entity_component_system/ecs.hpp>

#include <gtest/gtest.h>

namespace
{
struct SkeletalAnimationFixture
{
	SkeletalAnimationFixture()
	{
		Bone bone;
		bone.name = "root";
		skeleton_id = ecs.add_skeleton({ bone });

		BoneAnimation bone_animation;
		bone_animation.animation_start_secs = 0.0f;
		bone_animation.animation_end_secs = 1.0f;
		bone_animation.translation_track.keys = {
			{ 0.0f, glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(0.0f) },
			{ 1.0f, glm::vec3(2.0f, 0.0f, 0.0f), glm::vec3(0.0f), glm::vec3(0.0f) },
		};
		animation_id = ecs.add_skeletal_animation(
			"move", { std::move(bone_animation) }, make_skeletal_rig_signature({ bone }));
	}

	float bone_x() const
	{
		return ecs.get_skeletal_component(skeleton_id)
			.get_bones()[0].relative_transform.get_pos().x;
	}

	ECS ecs;
	SkeletonID skeleton_id;
	AnimationID animation_id;
};
}

TEST(SkeletalAnimationSystem, pauses_seeks_and_steps_within_clip_bounds)
{
	SkeletalAnimationFixture fixture;
	fixture.ecs.play_animation(fixture.skeleton_id, fixture.animation_id);
	fixture.ecs.process(0.25f);

	auto playback = fixture.ecs.get_animation_playback(fixture.skeleton_id);
	ASSERT_TRUE(playback);
	EXPECT_FLOAT_EQ(playback->elapsed_secs, 0.25f);
	EXPECT_FLOAT_EQ(playback->duration_secs, 1.0f);
	EXPECT_FLOAT_EQ(fixture.bone_x(), 0.5f);

	fixture.ecs.set_animation_paused(fixture.skeleton_id, true);
	fixture.ecs.process(0.25f);
	EXPECT_FLOAT_EQ(fixture.bone_x(), 0.5f);

	fixture.ecs.seek_animation(fixture.skeleton_id, 0.75f);
	EXPECT_FLOAT_EQ(fixture.bone_x(), 1.5f);
	fixture.ecs.step_animation(fixture.skeleton_id, 1.0f);
	EXPECT_FLOAT_EQ(fixture.ecs.get_animation_playback(fixture.skeleton_id)->elapsed_secs, 1.0f);
	EXPECT_FLOAT_EQ(fixture.bone_x(), 2.0f);
	fixture.ecs.step_animation(fixture.skeleton_id, -2.0f);
	EXPECT_FLOAT_EQ(fixture.ecs.get_animation_playback(fixture.skeleton_id)->elapsed_secs, 0.0f);
	EXPECT_FLOAT_EQ(fixture.bone_x(), 0.0f);
}

TEST(SkeletalAnimationSystem, applies_loop_changes_to_active_playback)
{
	SkeletalAnimationFixture fixture;
	fixture.ecs.play_animation(fixture.skeleton_id, fixture.animation_id, true);
	fixture.ecs.process(1.25f);

	auto playback = fixture.ecs.get_animation_playback(fixture.skeleton_id);
	ASSERT_TRUE(playback);
	EXPECT_TRUE(playback->looping);
	EXPECT_FLOAT_EQ(playback->elapsed_secs, 0.0f);

	fixture.ecs.set_animation_looping(fixture.skeleton_id, false);
	EXPECT_FALSE(fixture.ecs.get_animation_playback(fixture.skeleton_id)->looping);
	fixture.ecs.process(1.25f);
	EXPECT_FALSE(fixture.ecs.get_animation_playback(fixture.skeleton_id));
	EXPECT_FLOAT_EQ(fixture.bone_x(), 0.0f);
}

TEST(SkeletalAnimationSystem, supports_unrestricted_playback_speed)
{
	SkeletalAnimationFixture fixture;
	fixture.ecs.play_animation(fixture.skeleton_id, fixture.animation_id);
	fixture.ecs.set_animation_speed(fixture.skeleton_id, 0.5f);
	fixture.ecs.process(0.5f);

	auto playback = fixture.ecs.get_animation_playback(fixture.skeleton_id);
	ASSERT_TRUE(playback);
	EXPECT_FLOAT_EQ(playback->speed, 0.5f);
	EXPECT_FLOAT_EQ(playback->elapsed_secs, 0.25f);
	EXPECT_FLOAT_EQ(fixture.bone_x(), 0.5f);

	fixture.ecs.set_animation_speed(fixture.skeleton_id, 2.0f);
	EXPECT_FLOAT_EQ(fixture.ecs.get_animation_playback(fixture.skeleton_id)->speed, 2.0f);
	fixture.ecs.set_animation_speed(fixture.skeleton_id, 0.0f);
	EXPECT_FLOAT_EQ(fixture.ecs.get_animation_playback(fixture.skeleton_id)->speed, 0.0f);
}
