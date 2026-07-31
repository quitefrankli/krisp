#include <entity_component_system/ecs.hpp>
#include <serialization/resource_provenance.hpp>

#include <gtest/gtest.h>

#include <type_traits>

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

TEST(SkeletalSystem, exposes_only_const_bone_topology)
{
	static_assert(std::is_same_v<
		decltype(std::declval<SkeletalComponent&>().get_bones()),
		const std::vector<Bone>&>);
}

TEST(SkeletalAnimationSystem, pauses_seeks_and_steps_within_clip_bounds)
{
	SkeletalAnimationFixture fixture;
	EXPECT_TRUE(fixture.ecs.play_animation(fixture.skeleton_id, fixture.animation_id));
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

TEST(SkeletalAnimationSystem, rejects_invalid_play_requests_without_changing_playback)
{
	SkeletalAnimationFixture fixture;
	ASSERT_TRUE(fixture.ecs.play_animation(fixture.skeleton_id, fixture.animation_id, true));

	Bone incompatible_bone;
	incompatible_bone.name = "other";
	const auto incompatible_animation = fixture.ecs.add_skeletal_animation(
		"incompatible", { BoneAnimation{} }, make_skeletal_rig_signature({ incompatible_bone }));

	EXPECT_FALSE(fixture.ecs.play_animation(fixture.skeleton_id, incompatible_animation));
	EXPECT_FALSE(fixture.ecs.play_animation(fixture.skeleton_id, AnimationID(999999)));
	EXPECT_FALSE(fixture.ecs.play_animation(SkeletonID(999999), fixture.animation_id));

	const auto playback = fixture.ecs.get_animation_playback(fixture.skeleton_id);
	ASSERT_TRUE(playback);
	EXPECT_EQ(playback->animation_id, fixture.animation_id);
	EXPECT_TRUE(playback->looping);
}

TEST(SkeletalAnimationSystem, removes_animation_and_stops_active_playback)
{
	SkeletalAnimationFixture fixture;
	ResourceProvenance::register_animation(
		fixture.animation_id, { .source = "move.glb", .skin = 0, .animation = 0 });
	ASSERT_TRUE(fixture.ecs.play_animation(fixture.skeleton_id, fixture.animation_id));
	fixture.ecs.process(0.5f);
	ASSERT_NE(fixture.bone_x(), 0.0f);

	EXPECT_TRUE(fixture.ecs.remove_skeletal_animation(fixture.animation_id));
	EXPECT_FALSE(fixture.ecs.get_skeletal_animations().contains(fixture.animation_id));
	EXPECT_FALSE(fixture.ecs.get_animation_playback(fixture.skeleton_id));
	EXPECT_FLOAT_EQ(fixture.bone_x(), 0.0f);
	EXPECT_EQ(ResourceProvenance::animation(fixture.animation_id), nullptr);
	EXPECT_FALSE(fixture.ecs.remove_skeletal_animation(fixture.animation_id));
}

TEST(SkeletalAnimationSystem, applies_loop_changes_to_active_playback)
{
	SkeletalAnimationFixture fixture;
	fixture.ecs.play_animation(fixture.skeleton_id, fixture.animation_id, true);
	fixture.ecs.process(1.25f);

	auto playback = fixture.ecs.get_animation_playback(fixture.skeleton_id);
	ASSERT_TRUE(playback);
	EXPECT_TRUE(playback->looping);
	EXPECT_FLOAT_EQ(playback->elapsed_secs, 0.25f);
	EXPECT_FLOAT_EQ(fixture.bone_x(), 0.5f);

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

TEST(SkeletalAnimationSystem, crossfades_from_the_displayed_pose_and_can_be_interrupted)
{
	SkeletalAnimationFixture fixture;
	Bone bone;
	bone.name = "root";
	BoneAnimation target;
	target.animation_start_secs = 0.0f;
	target.animation_end_secs = 1.0f;
	target.translation_track.keys = {
		{ 0.0f, glm::vec3(4.0f, 0.0f, 0.0f), {}, {} },
		{ 1.0f, glm::vec3(4.0f, 0.0f, 0.0f), {}, {} },
	};
	const auto target_id = fixture.ecs.add_skeletal_animation(
		"target", { std::move(target) }, make_skeletal_rig_signature({ bone }));

	ASSERT_TRUE(fixture.ecs.play_animation(fixture.skeleton_id, fixture.animation_id));
	fixture.ecs.process(0.25f);
	ASSERT_TRUE(fixture.ecs.crossfade_animation(fixture.skeleton_id, target_id, 1.0f));
	fixture.ecs.process(0.5f);
	EXPECT_FLOAT_EQ(fixture.bone_x(), 2.25f); // mix(displayed 0.5, target 4.0, 0.5)

	ASSERT_TRUE(fixture.ecs.crossfade_animation(fixture.skeleton_id, fixture.animation_id, 0.5f));
	fixture.ecs.process(0.25f);
	EXPECT_FLOAT_EQ(fixture.bone_x(), 1.375f); // mix(interrupted pose 2.25, target 0.5, 0.5)
}

TEST(SkeletalAnimationSystem, crossfade_zero_duration_is_an_immediate_play)
{
	SkeletalAnimationFixture fixture;
	ASSERT_TRUE(fixture.ecs.play_animation(fixture.skeleton_id, fixture.animation_id));
	fixture.ecs.process(0.5f);
	ASSERT_TRUE(fixture.ecs.crossfade_animation(fixture.skeleton_id, fixture.animation_id, 0.0f, true));
	EXPECT_TRUE(fixture.ecs.get_animation_playback(fixture.skeleton_id)->looping);
	EXPECT_FLOAT_EQ(fixture.bone_x(), 0.0f);
}

TEST(SkeletalAnimationSystem, crossfade_uses_the_shortest_rotation_path)
{
	ECS ecs;
	Bone bone;
	bone.name = "root";
	const auto skeleton_id = ecs.add_skeleton({ bone });
	const auto make_rotation = [](const float degrees)
	{
		const auto rotation = glm::angleAxis(glm::radians(degrees), glm::vec3(0.0f, 0.0f, 1.0f));
		BoneAnimation animation;
		animation.animation_start_secs = 0.0f;
		animation.animation_end_secs = 1.0f;
		animation.rotation_track.keys = {
			{ 0.0f, glm::vec4(rotation.x, rotation.y, rotation.z, rotation.w), {}, {} },
			{ 1.0f, glm::vec4(rotation.x, rotation.y, rotation.z, rotation.w), {}, {} },
		};
		return animation;
	};
	const auto source_id = ecs.add_skeletal_animation(
		"left", { make_rotation(170.0f) }, make_skeletal_rig_signature({ bone }));
	const auto target_id = ecs.add_skeletal_animation(
		"right", { make_rotation(-170.0f) }, make_skeletal_rig_signature({ bone }));

	ASSERT_TRUE(ecs.play_animation(skeleton_id, source_id));
	ecs.process(0.0f);
	ASSERT_TRUE(ecs.crossfade_animation(skeleton_id, target_id, 1.0f));
	ecs.process(0.5f);
	const auto orientation = ecs.get_skeletal_component(skeleton_id).get_bones()[0]
		.relative_transform.get_orient();
	EXPECT_NEAR(std::abs(orientation.z), 1.0f, 0.0001f);
	EXPECT_NEAR(orientation.w, 0.0f, 0.0001f);
}

TEST(SkeletalAnimationSystem, loops_by_modulo_without_holding_the_end_pose)
{
	SkeletalAnimationFixture fixture;
	ASSERT_TRUE(fixture.ecs.play_animation(fixture.skeleton_id, fixture.animation_id, true));
	fixture.ecs.process(1.25f);
	const auto playback = fixture.ecs.get_animation_playback(fixture.skeleton_id);
	ASSERT_TRUE(playback);
	EXPECT_FLOAT_EQ(playback->elapsed_secs, 0.25f);
	EXPECT_FLOAT_EQ(fixture.bone_x(), 0.5f);
}

TEST(SkeletalAnimationSystem, aligns_cubic_rotation_keys_and_tangents_to_one_hemisphere)
{
	ECS ecs;
	Bone bone;
	bone.name = "root";
	const auto skeleton_id = ecs.add_skeleton({ bone });

	BoneAnimation animation;
	animation.animation_start_secs = 0.0f;
	animation.animation_end_secs = 1.0f;
	animation.rotation_track.interpolation = BoneAnimation::Interpolation::CUBIC_SPLINE;
	// q and -q represent the same orientation, but cubic interpolation between
	// opposite signs approaches a zero quaternion unless the key and its tangents are flipped.
	animation.rotation_track.keys = {
		{ 0.0f, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f), {}, {} },
		{ 1.0f, glm::vec4(0.0f, 0.0f, 0.0f, -1.0f),
			glm::vec4(0.25f, 0.0f, 0.0f, 0.0f),
			glm::vec4(0.5f, 0.0f, 0.0f, 0.0f) },
	};
	const auto animation_id = ecs.add_skeletal_animation(
		"antipodal", { std::move(animation) }, make_skeletal_rig_signature({ bone }));

	const auto& keys = ecs.get_skeletal_animations().at(animation_id).bone_animations[0].rotation_track.keys;
	ASSERT_EQ(keys.size(), 2u);
	EXPECT_FLOAT_EQ(keys[1].value.w, 1.0f);
	EXPECT_FLOAT_EQ(keys[1].in_tangent.x, -0.25f);
	EXPECT_FLOAT_EQ(keys[1].out_tangent.x, -0.5f);

	ecs.play_animation(skeleton_id, animation_id);
	ecs.seek_animation(skeleton_id, 0.5f);
	const auto orientation =
		ecs.get_skeletal_component(skeleton_id).get_bones()[0].relative_transform.get_orient();
	EXPECT_TRUE(std::isfinite(orientation.x));
	EXPECT_TRUE(std::isfinite(orientation.y));
	EXPECT_TRUE(std::isfinite(orientation.z));
	EXPECT_TRUE(std::isfinite(orientation.w));
	EXPECT_NEAR(orientation.x, 0.0f, 0.0001f);
	EXPECT_NEAR(std::abs(orientation.w), 1.0f, 0.0001f);
}
