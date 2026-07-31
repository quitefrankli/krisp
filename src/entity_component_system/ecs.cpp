#include "ecs.hpp"
#include "serialization/serializer.hpp"


void ECS::process(const float delta_secs)
{
	SkeletalAnimationSystem::process(delta_secs);
	SkeletalSystem::process(delta_secs);
	PhysicsSystem::process(delta_secs);
	ParticleSystem::process(delta_secs);
}

void ECS::serialize(Serializer& out) const
{
	TransformationSystem::serialize(out);
	ClickableSystem::serialize(out);
	HoverableSystem::serialize(out);
	LightSystem::serialize(out);
	ColliderSystem::serialize(out);
	PhysicsSystem::serialize(out);
	SkeletalSystem::serialize(out);
	RenderableSystem::serialize(out);
	SkeletalAnimationSystem::serialize(out);
	EquipmentSystem::serialize(out);
	TileSystem::serialize(out);
}

void ECS::deserialize(const Deserializer& in)
{
	TransformationSystem::deserialize(in);
	ClickableSystem::deserialize(in);
	HoverableSystem::deserialize(in);
	LightSystem::deserialize(in);
	ColliderSystem::deserialize(in);
	PhysicsSystem::deserialize(in);
	SkeletalSystem::deserialize(in);
	RenderableSystem::deserialize(in);
	SkeletalSystem::deserialize_bone_attachments(in);
	SkeletalAnimationSystem::deserialize(in);
	EquipmentSystem::deserialize(in);
	TileSystem::deserialize(in);
}

void ECS::add_object(Object& object)
{
	objects.emplace(object.get_id(), &object);
	add_transformation(
		object.get_id(),
		object.is_transient()
			? TransformationPersistence::Transient
			: TransformationPersistence::Persistent);
}

void ECS::remove_object(const ObjectID id) 
{
	EquipmentSystem::remove_entity(id);
	SkeletalSystem::remove_entity(id);
	RenderableSystem::remove_object_renderables(id);
	LightSystem::remove_entity(id);
	ColliderSystem::remove_entity(id);
	ClickableSystem::remove_entity(id);
	PhysicsSystem::remove_entity(id);
	ParticleSystem::remove_entity(id);
	TileSystem::remove_entity(id);
	objects.erase(id);
	remove_transformation(id);
}

void ECS::reset_preserving_transient_transformations()
{
	auto transformations = take_transient_transformations();
	auto renderables = take_renderables_if([&transformations](const ObjectID id) {
		return transformations.has_transformation(id);
	});
	std::unordered_map<ObjectID, Object*> transient_objects;
	for (const auto& [id, object] : objects)
		if (transformations.has_transformation(id))
			transient_objects.emplace(id, object);
	*this = ECS{};
	static_cast<TransformationSystem&>(*this) = std::move(transformations);
	objects = std::move(transient_objects);
	restore_renderables(std::move(renderables));
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
