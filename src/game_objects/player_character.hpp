#pragma once

#include "game_objects/character.hpp"
#include "input.hpp"
#include <glm/vec3.hpp>

class Camera;
class ECS;

struct PlayerDefinition
{
	float movement_speed = 3.5f;
	float capsule_radius = 0.35f;
	float capsule_height = 1.7f;
	float ground_snap_distance = 1.0f;
	glm::vec3 camera_focus_offset{ 0.0f, 1.4f, 0.0f };
};

// A reusable third-person character with camera-relative movement.
class PlayerCharacter : public Character
{
public:
	PlayerCharacter(std::vector<Renderable> renderables, PlayerDefinition definition);

	static glm::vec3 movement_direction(
		bool forward, bool backward, bool right, bool left,
		const glm::vec3& camera_forward, const glm::vec3& camera_right);

	void pre_update(const Keyboard& keyboard, const Camera& camera, ECS& ecs, float delta_secs);
	bool is_moving() const { return moving; }
	const PlayerDefinition& get_definition() const { return definition; }

private:
	void resolve_horizontal_movement(ECS& ecs, glm::vec3 displacement);
	void snap_to_ground(ECS& ecs);

	PlayerDefinition definition;
	bool moving = false;
};
