#include "game_objects/player_character.hpp"
#include "game_objects/character.hpp"
#include "test_helper.hpp"
#include "camera.hpp"
#include "entity_component_system/ecs.hpp"
#include "collision/collider.hpp"

#include <GLFW/glfw3.h>

#include <gtest/gtest.h>

#include <array>

TEST(player_character_tests, camera_relative_input_is_normalized)
{
	const glm::vec3 direction = PlayerCharacter::movement_direction(
		true, false, true, false, Maths::forward_vec, Maths::right_vec);
	EXPECT_NEAR(glm::length(direction), 1.0f, 0.0001f);
	EXPECT_NEAR(direction.x, 0.7071067f, 0.0001f);
	EXPECT_NEAR(direction.z, 0.7071067f, 0.0001f);
}

TEST(player_character_tests, opposite_input_cancels_movement)
{
	const glm::vec3 direction = PlayerCharacter::movement_direction(
		true, true, false, false, Maths::forward_vec, Maths::right_vec);
	EXPECT_EQ(direction, Maths::zero_vec);
}

TEST(player_character_tests, input_is_projected_onto_the_horizontal_camera_plane)
{
	const glm::vec3 direction = PlayerCharacter::movement_direction(
		true, false, false, false,
		glm::normalize(glm::vec3(0.0f, 1.0f, 1.0f)), Maths::right_vec);
	EXPECT_TRUE(glm_equal(direction, Maths::forward_vec));
}

TEST(character_tests, looping_animation_is_retained_until_the_clip_changes)
{
	ECS ecs;
	Bone root;
	root.name = "Root";
	root.original_transform = root.relative_transform;
	const SkeletonID skeleton = ecs.add_skeleton({ root });
	const auto rig = make_skeletal_rig_signature(ecs.get_skeletal_component(skeleton).get_bones());
	const AnimationID idle = ecs.add_skeletal_animation("Idle", { BoneAnimation{} }, rig);
	const AnimationID walk = ecs.add_skeletal_animation("Walk", { BoneAnimation{} }, rig);
	Character character({});
	ecs.add_object(character);
	ecs.attach_skeleton(character.get_id(), skeleton);

	character.play_looping_animation(ecs, idle);
	character.play_looping_animation(ecs, idle);
	EXPECT_EQ(character.get_active_animation(), idle);
	character.play_looping_animation(ecs, walk);
	EXPECT_EQ(character.get_active_animation(), walk);

	ecs.remove_object(character.get_id());
}

TEST(player_character_tests, player_moves_at_configured_speed_and_changes_state)
{
	ECS ecs;
	Bone root;
	root.name = "Root";
	root.original_transform = root.relative_transform;
	const SkeletonID skeleton = ecs.add_skeleton({ root });
	const auto rig = make_skeletal_rig_signature(ecs.get_skeletal_component(skeleton).get_bones());
	const AnimationID idle = ecs.add_skeletal_animation("Idle", { BoneAnimation{} }, rig);
	const AnimationID walk = ecs.add_skeletal_animation("Walk", { BoneAnimation{} }, rig);
	PlayerDefinition definition;
	definition.idle_animation = idle;
	definition.walk_animation = walk;
	definition.movement_speed = 2.0f;
	PlayerCharacter player({}, definition);
	ecs.add_object(player);
	ecs.attach_skeleton(player.get_id(), skeleton);

	Camera camera(Listener{}, 1.0f);
	camera.look_at(Maths::forward_vec, glm::vec3(0.0f, 0.0f, -2.0f));
	Keyboard keyboard;
	keyboard.update_key({ GLFW_KEY_W, EKeyModifier::NONE, EInputAction::PRESS });
	player.pre_update(keyboard, camera, ecs, 0.5f);

	EXPECT_TRUE(player.is_moving());
	EXPECT_TRUE(glm_equal(player.get_position(), Maths::forward_vec));
	keyboard.update_key({ GLFW_KEY_W, EKeyModifier::NONE, EInputAction::RELEASE });
	player.pre_update(keyboard, camera, ecs, 0.5f);
	EXPECT_FALSE(player.is_moving());
	EXPECT_TRUE(glm_equal(player.get_position(), Maths::forward_vec));

	ecs.remove_object(player.get_id());
}

TEST(gameplay_collision_tests, raycast_returns_nearest_hit_and_honours_exclusion)
{
	ECS ecs;
	Object near_object;
	near_object.set_position({ 0.0f, 0.0f, 2.0f });
	Object far_object;
	far_object.set_position({ 0.0f, 0.0f, 5.0f });
	ecs.add_object(near_object);
	ecs.add_object(far_object);
	ecs.add_collider(near_object.get_id(), std::make_unique<BoxCollider>());
	ecs.add_collider(far_object.get_id(), std::make_unique<BoxCollider>());

	const Maths::Ray ray(Maths::zero_vec, Maths::forward_vec);
	EXPECT_EQ(ecs.raycast(ray).id, near_object.get_id());
	EXPECT_EQ(ecs.raycast(ray, near_object.get_id()).id, far_object.get_id());

	ecs.remove_object(near_object.get_id());
	ecs.remove_object(far_object.get_id());
}

TEST(gameplay_collision_tests, candidate_raycast_only_considers_supplied_entities)
{
	ECS ecs;
	Object excluded_near;
	excluded_near.set_position({ 0.0f, 0.0f, 1.0f });
	Object included_near;
	included_near.set_position({ 0.0f, 0.0f, 3.0f });
	Object included_far;
	included_far.set_position({ 0.0f, 0.0f, 6.0f });
	for (Object* object : { &excluded_near, &included_near, &included_far })
	{
		ecs.add_object(*object);
		ecs.add_collider(object->get_id(), std::make_unique<BoxCollider>());
	}

	const std::array candidates = { included_far.get_id(), included_near.get_id() };
	const auto hit = ecs.raycast(Maths::Ray(Maths::zero_vec, Maths::forward_vec), candidates);
	ASSERT_TRUE(hit.bCollided);
	EXPECT_EQ(hit.id, included_near.get_id());
}

TEST(gameplay_collision_tests, transient_colliders_require_an_explicit_candidate_raycast)
{
	ECS ecs;
	Object object;
	object.set_position({ 0.0f, 0.0f, 2.0f });
	const EntityID transient_id = ecs.add_transient_object(object);
	ecs.add_collider(transient_id, std::make_unique<BoxCollider>(), {}, ColliderPersistence::Transient);
	const Maths::Ray ray(Maths::zero_vec, Maths::forward_vec);

	EXPECT_FALSE(ecs.raycast(ray).bCollided);
	const std::array candidates = { transient_id };
	EXPECT_EQ(ecs.raycast(ray, candidates).id, transient_id);
}

TEST(skeletal_component_tests, model_space_transforms_compose_parent_hierarchy)
{
	Bone root;
	root.name = "Root";
	root.relative_transform.set_pos({ 1.0f, 0.0f, 0.0f });
	Bone child;
	child.name = "Child";
	child.parent_node = 0;
	child.relative_transform.set_pos({ 0.0f, 2.0f, 0.0f });
	SkeletalComponent component({ root, child });

	const auto transforms = component.get_model_space_bone_transforms();
	ASSERT_EQ(transforms.size(), 2u);
	EXPECT_TRUE(glm_equal(glm::vec3(transforms[0][3]), glm::vec3(1.0f, 0.0f, 0.0f)));
	EXPECT_TRUE(glm_equal(glm::vec3(transforms[1][3]), glm::vec3(1.0f, 2.0f, 0.0f)));
}
