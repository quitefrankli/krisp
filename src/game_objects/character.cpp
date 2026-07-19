#include "game_objects/character.hpp"

#include "entity_component_system/ecs.hpp"

#include <stdexcept>

Character::Character(std::vector<Renderable> renderables) :
	Object(renderables)
{
}

void Character::play_looping_animation(ECS& ecs, const AnimationID animation)
{
	if (has_animation && active_animation == animation)
		return;
	const auto skeleton = ecs.get_skeleton_id(get_id());
	if (!skeleton)
		throw std::runtime_error("Character requires a skeleton");
	ecs.play_animation(*skeleton, animation, true);
	active_animation = animation;
	has_animation = true;
}
