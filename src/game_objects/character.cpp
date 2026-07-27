#include "game_objects/character.hpp"

#include "entity_component_system/ecs.hpp"

#include <stdexcept>
#include <utility>

Character::Character(std::vector<Renderable> renderables) :
	Object(std::move(renderables))
{
}

void Character::play_looping_animation(ECS& ecs, const AnimationID animation)
{
	const auto skeleton = ecs.get_skeleton_id(get_id());
	if (!skeleton)
		throw std::runtime_error("Character requires a skeleton");
	play_looping_animation(ecs, *skeleton, animation, 0.0f);
}

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
