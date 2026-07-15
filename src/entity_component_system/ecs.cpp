#include "ecs.hpp"


void ECS::process(const float delta_secs)
{
	AnimationSystem::process(delta_secs);
	SkeletalAnimationSystem::process(delta_secs);
	PhysicsSystem::process(delta_secs);
	ParticleSystem::process(delta_secs);
}

ECS& ECS::get()
{
	static ECS ecs;
	return ecs;
}

void ECS::remove_object(const ObjectID id) 
{
	// Stop skeletal animations before removing their skeletons. Otherwise the
	// next animation tick retains a stale SkeletonID and accesses erased data.
	SkeletalAnimationSystem::remove_entity(id);
	SkeletalSystem::remove_entity(id);
	AnimationSystem::remove_entity(id);
	LightSystem::remove_entity(id);
	ColliderSystem::remove_entity(id);
	ClickableSystem::remove_entity(id);
	PhysicsSystem::remove_entity(id);
	ParticleSystem::remove_entity(id);
	TileSystem::remove_entity(id);
	objects.erase(id);
}

Object& ECS::get_object(const ObjectID id)
{
	assert(objects.find(id) != objects.end());
	return *objects.at(id);
}

const Object& ECS::get_object(const ObjectID id) const
{
	assert(objects.find(id) != objects.end());
	return *objects.at(id);
}
