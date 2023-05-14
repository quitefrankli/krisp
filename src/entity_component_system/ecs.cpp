#include "ecs.hpp"


ECS::ECS()
{
	// components[ECSComponentType::ANIMATION] = std::make_unique<AnimationSystem>();
}

ECS::~ECS()
{
}

void ECS::process(const float delta_secs)
{
	AnimationSystem::process(delta_secs);
}

void ECS::remove_object(const ObjectID id) 
{
	objects.erase(id);
	SkeletalSystem::remove_entity(id);
	AnimationSystem::remove_entity(id);
	LightSystem::remove_entity(id);
}
