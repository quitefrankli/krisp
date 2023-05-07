#include "ecs_manager.hpp"


ECSManager::ECSManager() :
	animation(*this)
{
	// components[ECSComponentType::ANIMATION] = std::make_unique<AnimationECS>();
}

ECSManager::~ECSManager()
{
}

void ECSManager::process(const float delta_secs)
{
	// components[ECSComponentType::ANIMATION]->process(delta_secs);
	animation.process(delta_secs);
}

void ECSManager::remove_object(const ObjectID id) 
{
	objects.erase(id);
	animation.remove_entity(id);
}
