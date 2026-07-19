#include "game_objects/player_character.hpp"

#include "camera.hpp"
#include "entity_component_system/ecs.hpp"

#include <glm/gtx/quaternion.hpp>

#include <algorithm>
#include <unordered_map>

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

void PlayerCharacter::pre_update(const Keyboard& keyboard, const Camera& camera, ECS& ecs, const float delta_secs)
{
	glm::vec3 forward = camera.get_focus() - camera.get_position();
	forward.y = 0.0f;
	if (glm::length2(forward) <= Maths::ACCEPTABLE_FLOATING_PT_DIFF)
		forward = Maths::forward_vec;
	else
		forward = glm::normalize(forward);
	const glm::vec3 right = glm::normalize(glm::cross(Maths::up_vec, forward));
	const glm::vec3 direction = movement_direction(
		keyboard.w_pressed(), keyboard.s_pressed(), keyboard.d_pressed(), keyboard.a_pressed(), forward, right);
	moving = glm::length2(direction) > Maths::ACCEPTABLE_FLOATING_PT_DIFF;
	if (moving)
	{
		set_rotation(Maths::Vec2Rot(direction));
		resolve_horizontal_movement(ecs, direction * definition.movement_speed * delta_secs);
	}
	snap_to_ground(ecs);
	play_looping_animation(ecs, moving ? definition.walk_animation : definition.idle_animation);
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

void PlayerCharacter::post_animation_update(ECS& ecs)
{
	solve_leg_ik(ecs, definition.left_leg);
	solve_leg_ik(ecs, definition.right_leg);
}

void PlayerCharacter::solve_leg_ik(ECS& ecs, const PlayerLegDefinition& leg)
{
	if (leg.hip_bone.empty() || leg.knee_bone.empty() || leg.foot_bone.empty())
		return;
	const auto skeleton_id = ecs.get_skeleton_id(get_id());
	if (!skeleton_id)
		return;
	auto& component = ecs.get_skeletal_component(*skeleton_id);
	auto& bones = component.get_bones();
	std::unordered_map<std::string, size_t> indices;
	for (size_t i = 0; i < bones.size(); ++i)
		indices.emplace(bones[i].name, i);
	const auto hip = indices.find(leg.hip_bone);
	const auto knee = indices.find(leg.knee_bone);
	const auto foot = indices.find(leg.foot_bone);
	if (hip == indices.end() || knee == indices.end() || foot == indices.end())
		return;

	auto transforms = component.get_model_space_bone_transforms();
	const glm::vec3 foot_world = glm::vec3(get_transform() * transforms[foot->second] * glm::vec4(0, 0, 0, 1));
	Maths::Ray ray(foot_world + Maths::up_vec * definition.ground_snap_distance, -Maths::up_vec);
	ray.length = definition.ground_snap_distance * 2.0f;
	const auto hit = ecs.raycast(ray, get_id());
	if (!hit.bCollided || glm::distance(ray.origin, hit.intersection) > ray.length)
		return;

	const glm::vec3 target = glm::vec3(glm::inverse(get_transform()) * glm::vec4(hit.intersection, 1.0f));
	const glm::vec3 hip_pos = glm::vec3(transforms[hip->second][3]);
	const glm::vec3 foot_pos = glm::vec3(transforms[foot->second][3]);
	const glm::vec3 current = foot_pos - hip_pos;
	const glm::vec3 desired = target - hip_pos;
	if (glm::length2(current) <= Maths::ACCEPTABLE_FLOATING_PT_DIFF ||
		glm::length2(desired) <= Maths::ACCEPTABLE_FLOATING_PT_DIFF)
		return;
	// First CCD pass at the hip. The subsequent knee pass preserves the animated
	// bone lengths while placing the foot as close as its reachable chain allows.
	const glm::quat model_delta = glm::rotation(glm::normalize(current), glm::normalize(desired));
	const uint32_t parent = bones[hip->second].parent_node;
	const glm::quat parent_rotation = parent == Bone::NO_PARENT
		? Maths::identity_quat : glm::quat_cast(transforms[parent]);
	const glm::quat local_delta = glm::inverse(parent_rotation) * model_delta * parent_rotation;
	bones[hip->second].relative_transform.set_orient(
		glm::normalize(local_delta * bones[hip->second].relative_transform.get_orient()));

	transforms = component.get_model_space_bone_transforms();
	const glm::vec3 knee_pos = glm::vec3(transforms[knee->second][3]);
	const glm::vec3 adjusted_foot = glm::vec3(transforms[foot->second][3]);
	const glm::vec3 knee_current = adjusted_foot - knee_pos;
	const glm::vec3 knee_desired = target - knee_pos;
	if (glm::length2(knee_current) <= Maths::ACCEPTABLE_FLOATING_PT_DIFF ||
		glm::length2(knee_desired) <= Maths::ACCEPTABLE_FLOATING_PT_DIFF)
		return;
	const glm::quat knee_model_delta = glm::rotation(glm::normalize(knee_current), glm::normalize(knee_desired));
	const uint32_t knee_parent = bones[knee->second].parent_node;
	const glm::quat knee_parent_rotation = knee_parent == Bone::NO_PARENT
		? Maths::identity_quat : glm::quat_cast(transforms[knee_parent]);
	const glm::quat knee_local_delta = glm::inverse(knee_parent_rotation) * knee_model_delta * knee_parent_rotation;
	bones[knee->second].relative_transform.set_orient(
		glm::normalize(knee_local_delta * bones[knee->second].relative_transform.get_orient()));
}
