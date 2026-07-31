#include "game_objects/character.hpp"

#include "entity_component_system/ecs.hpp"

#include <stdexcept>
#include <utility>

void Character::play_looping_animation(
	ECS& ecs,
	const SkeletonID skeleton,
	const AnimationID animation,
	const float transition_secs)
{
	if (has_animation && active_animation == animation)
		return;
	const bool started = transition_secs > 0.0f
		? ecs.crossfade_animation(skeleton, animation, transition_secs, true)
		: ecs.play_animation(skeleton, animation, true);
	if (started)
	{
		active_animation = animation;
		has_animation = true;
	}
}

bool Character::play_one_shot_animation(
	ECS& ecs,
	const SkeletonID skeleton,
	const AnimationID animation,
	const float transition_secs)
{
	const bool started = transition_secs > 0.0f
		? ecs.crossfade_animation(skeleton, animation, transition_secs, false)
		: ecs.play_animation(skeleton, animation, false);
	if (started)
	{
		active_animation = animation;
		has_animation = true;
	}
	return started;
}
