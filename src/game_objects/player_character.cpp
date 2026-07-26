#include "game_objects/player_character.hpp"

#include "camera.hpp"
#include "entity_component_system/ecs.hpp"

#include <algorithm>

PlayerCharacter::PlayerCharacter(
	std::vector<Renderable> renderables,
	PlayerDefinition definition) :
	Character(renderables),
	definition(std::move(definition))
{
}

glm::vec3 PlayerCharacter::movement_direction(
	const bool forward, const bool backward, const bool right, const bool left,
	const glm::vec3& camera_forward, const glm::vec3& camera_right)
{
	glm::vec3 result = (forward ? camera_forward : Maths::zero_vec) -
		(backward ? camera_forward : Maths::zero_vec) +
		(right ? camera_right : Maths::zero_vec) -
		(left ? camera_right : Maths::zero_vec);
	result.y = 0.0f;
	return glm::length2(result) <= Maths::ACCEPTABLE_FLOATING_PT_DIFF
		? Maths::zero_vec : glm::normalize(result);
}

PlayerLocomotionDirection PlayerCharacter::locomotion_direction(
	const bool forward, const bool backward, const bool right, const bool left)
{
	const int longitudinal = static_cast<int>(forward) - static_cast<int>(backward);
	const int lateral = static_cast<int>(right) - static_cast<int>(left);
	if (longitudinal > 0 && lateral > 0) return PlayerLocomotionDirection::FORWARD_RIGHT;
	if (longitudinal > 0 && lateral < 0) return PlayerLocomotionDirection::FORWARD_LEFT;
	if (longitudinal < 0 && lateral > 0) return PlayerLocomotionDirection::BACKWARD_RIGHT;
	if (longitudinal < 0 && lateral < 0) return PlayerLocomotionDirection::BACKWARD_LEFT;
	if (longitudinal > 0) return PlayerLocomotionDirection::FORWARD;
	if (longitudinal < 0) return PlayerLocomotionDirection::BACKWARD;
	if (lateral > 0) return PlayerLocomotionDirection::RIGHT;
	if (lateral < 0) return PlayerLocomotionDirection::LEFT;
	return PlayerLocomotionDirection::IDLE;
}

void PlayerCharacter::configure_locomotion(
	const SkeletonID skeleton,
	PlayerLocomotionAnimations animations)
{
	animation_skeleton = skeleton;
	locomotion_animations = std::move(animations);
}

AnimationID PlayerCharacter::animation_for(const PlayerLocomotionDirection direction) const
{
	const auto& animations = *locomotion_animations;
	switch (direction)
	{
		case PlayerLocomotionDirection::IDLE: return animations.idle;
		case PlayerLocomotionDirection::FORWARD: return animations.walk_forward;
		case PlayerLocomotionDirection::BACKWARD: return animations.walk_backward;
		case PlayerLocomotionDirection::LEFT: return animations.walk_left;
		case PlayerLocomotionDirection::RIGHT: return animations.walk_right;
		case PlayerLocomotionDirection::FORWARD_LEFT: return animations.walk_forward_left;
		case PlayerLocomotionDirection::FORWARD_RIGHT: return animations.walk_forward_right;
		case PlayerLocomotionDirection::BACKWARD_LEFT: return animations.walk_backward_left;
		case PlayerLocomotionDirection::BACKWARD_RIGHT: return animations.walk_backward_right;
	}
	return animations.idle;
}

void PlayerCharacter::pre_update(const Keyboard& keyboard, const Camera& camera, ECS& ecs, const float delta_secs)
{
	glm::vec3 forward = camera.get_focus() - camera.get_position();
	forward.y = 0.0f;
	if (glm::length2(forward) <= Maths::ACCEPTABLE_FLOATING_PT_DIFF)
		forward = Maths::forward_vec;
	else
		forward = glm::normalize(forward);
	const glm::vec3 right = glm::normalize(glm::cross(Maths::up_vec, forward));
	const bool forward_pressed = keyboard.w_pressed();
	const bool backward_pressed = keyboard.s_pressed();
	const bool right_pressed = keyboard.d_pressed();
	const bool left_pressed = keyboard.a_pressed();
	const glm::vec3 direction = movement_direction(
		forward_pressed, backward_pressed, right_pressed, left_pressed, forward, right);
	moving = glm::length2(direction) > Maths::ACCEPTABLE_FLOATING_PT_DIFF;
	if (moving)
	{
		set_rotation(Maths::Vec2Rot(forward));
		resolve_horizontal_movement(ecs, direction * definition.movement_speed * delta_secs);
	}
	if (animation_skeleton && locomotion_animations)
		play_looping_animation(ecs, *animation_skeleton,
			animation_for(locomotion_direction(
				forward_pressed, backward_pressed, right_pressed, left_pressed)),
			definition.animation_transition_secs);
	snap_to_ground(ecs);
}

void PlayerCharacter::resolve_horizontal_movement(ECS& ecs, glm::vec3 displacement)
{
	const float distance = glm::length(displacement);
	if (distance <= Maths::ACCEPTABLE_FLOATING_PT_DIFF)
		return;
	const glm::vec3 direction = displacement / distance;
	// Three probes approximate the controller capsule's lower, middle, and upper
	// sections. A blocked move is projected along the contact normal for sliding.
	for (const float height : { definition.capsule_radius, definition.capsule_height * 0.5f,
		definition.capsule_height - definition.capsule_radius })
	{
		Maths::Ray ray(get_position() + Maths::up_vec * height, direction);
		ray.length = distance + definition.capsule_radius;
		const auto hit = ecs.raycast(ray, get_id());
		if (!hit.bCollided || glm::distance(get_position() + Maths::up_vec * height, hit.intersection) > ray.length)
			continue;
		glm::vec3 normal = get_position() - hit.intersection;
		normal.y = 0.0f;
		if (glm::length2(normal) <= Maths::ACCEPTABLE_FLOATING_PT_DIFF)
			return;
		normal = glm::normalize(normal);
		displacement -= normal * glm::dot(displacement, normal);
		break;
	}
	set_position(get_position() + displacement);
}

void PlayerCharacter::snap_to_ground(ECS& ecs)
{
	Maths::Ray ray(get_position() + Maths::up_vec * definition.ground_snap_distance, -Maths::up_vec);
	ray.length = definition.ground_snap_distance + definition.capsule_height;
	const auto hit = ecs.raycast(ray, get_id());
	if (hit.bCollided && glm::distance(ray.origin, hit.intersection) <= ray.length)
	{
		auto position = get_position();
		position.y = hit.intersection.y;
		set_position(position);
	}
}
