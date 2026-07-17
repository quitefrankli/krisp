#pragma once

#include "game_objects/character.hpp"
#include "input.hpp"
#include "identifications.hpp"

#include <glm/vec3.hpp>

#include <string>

class Camera;
class ECS;

struct PlayerLegDefinition
{
	std::string hip_bone;
	std::string knee_bone;
	std::string foot_bone;
};

struct PlayerDefinition
{
	AnimationID idle_animation;
	AnimationID walk_animation;
	float movement_speed = 3.5f;
	float capsule_radius = 0.35f;
	float capsule_height = 1.7f;
	float ground_snap_distance = 1.0f;
	glm::vec3 camera_focus_offset{ 0.0f, 1.4f, 0.0f };
	PlayerLegDefinition left_leg;
	PlayerLegDefinition right_leg;
};

// A reusable in-place, third-person character. Movement is prepared before
// ECS animation sampling; foot IK is applied afterwards.
class PlayerCharacter : public Character
{
public:
	PlayerCharacter(std::vector<Renderable> renderables, SkeletonID skeleton_id, PlayerDefinition definition);

	static glm::vec3 movement_direction(
		bool forward, bool backward, bool right, bool left,
		const glm::vec3& camera_forward, const glm::vec3& camera_right);

	void pre_update(const Keyboard& keyboard, const Camera& camera, ECS& ecs, float delta_secs);
	void post_animation_update(ECS& ecs);
	bool is_moving() const { return moving; }
	const PlayerDefinition& get_definition() const { return definition; }

private:
	void resolve_horizontal_movement(ECS& ecs, glm::vec3 displacement);
	void snap_to_ground(ECS& ecs);
	void solve_leg_ik(ECS& ecs, const PlayerLegDefinition& leg);

	PlayerDefinition definition;
	bool moving = false;
};
