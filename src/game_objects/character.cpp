#include "game_objects/character.hpp"

#include "entity_component_system/ecs.hpp"

#include <stdexcept>

Character::Character(std::vector<Renderable> renderables, const SkeletonID skeleton_id) :
	Object(renderables, skeleton_id)
{
}

void Character::play_looping_animation(ECS& ecs, const AnimationID animation)
{
	if (has_animation && active_animation == animation)
		return;
	const auto skeleton = get_skeleton_id();
	if (!skeleton)
		throw std::runtime_error("Character requires a skeleton");
	ecs.play_animation(*skeleton, animation, true);
	active_animation = animation;
	has_animation = true;
}
