#include "ecs.hpp"
#include "serialization/serializer.hpp"


void ECS::process(const float delta_secs)
{
	AnimationSystem::process(delta_secs);
	SkeletalAnimationSystem::process(delta_secs);
	PhysicsSystem::process(delta_secs);
	ParticleSystem::process(delta_secs);
}

void ECS::serialize(Serializer& out) const
{
	ClickableSystem::serialize(out);
	HoverableSystem::serialize(out);
	LightSystem::serialize(out);
	ColliderSystem::serialize(out);
	PhysicsSystem::serialize(out);
	AnimationSystem::serialize(out);
	SkeletalSystem::serialize(out);
	SkeletalAnimationSystem::serialize(out);
	TileSystem::serialize(out);
}

void ECS::deserialize(const Deserializer& in)
{
	ClickableSystem::deserialize(in);
	HoverableSystem::deserialize(in);
	LightSystem::deserialize(in);
	ColliderSystem::deserialize(in);
	PhysicsSystem::deserialize(in);
	AnimationSystem::deserialize(in);
	SkeletalSystem::deserialize(in);
	SkeletalAnimationSystem::deserialize(in);
	TileSystem::deserialize(in);
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
