#pragma once

#include "game_objects/character.hpp"
#include "input.hpp"
#include <glm/vec3.hpp>

#include <optional>

class Camera;
class ECS;

struct PlayerDefinition
{
	float movement_speed = 3.5f;
	float capsule_radius = 0.35f;
	float capsule_height = 1.7f;
	float ground_snap_distance = 1.0f;
	glm::vec3 camera_focus_offset{ 0.0f, 1.4f, 0.0f };
	float camera_horizontal_offset = 1.25f;
	float animation_transition_secs = 0.2f;
};

enum class PlayerLocomotionDirection
{
	IDLE,
	FORWARD,
	BACKWARD,
	LEFT,
	RIGHT,
	FORWARD_LEFT,
	FORWARD_RIGHT,
	BACKWARD_LEFT,
	BACKWARD_RIGHT,
};

struct PlayerLocomotionAnimations
{
	AnimationID idle;
	AnimationID walk_backward;
	AnimationID walk_backward_left;
	AnimationID walk_backward_right;
	AnimationID walk_forward;
	AnimationID walk_forward_left;
	AnimationID walk_forward_right;
	AnimationID walk_left;
	AnimationID walk_right;
};

// A reusable third-person character with camera-relative movement.
class PlayerCharacter : public Character
{
public:
	explicit PlayerCharacter(PlayerDefinition definition);

	static glm::vec3 movement_direction(
		bool forward, bool backward, bool right, bool left,
		const glm::vec3& camera_forward, const glm::vec3& camera_right);
	static PlayerLocomotionDirection locomotion_direction(
		bool forward, bool backward, bool right, bool left);

	void pre_update(const Keyboard& keyboard, const Camera& camera, ECS& ecs, float delta_secs);
	void configure_locomotion(SkeletonID skeleton, PlayerLocomotionAnimations animations);
	bool play_action_animation(ECS& ecs, AnimationID animation);
	bool is_moving() const { return moving; }
	const PlayerDefinition& get_definition() const { return definition; }

private:
	void resolve_horizontal_movement(ECS& ecs, glm::vec3 displacement);
	void snap_to_ground(ECS& ecs);
	AnimationID animation_for(PlayerLocomotionDirection direction) const;

	PlayerDefinition definition;
	std::optional<SkeletonID> animation_skeleton;
	std::optional<PlayerLocomotionAnimations> locomotion_animations;
	std::optional<AnimationID> action_animation;
	bool moving = false;
};
