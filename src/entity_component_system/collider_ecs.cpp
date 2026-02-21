#include "collider_ecs.hpp"
#include "ecs.hpp"


void ColliderSystem::add_collider(EntityID id, std::unique_ptr<Collider>&& collider) 
{
	add_collider(id, std::move(collider), Maths::Transform{});
}

void ColliderSystem::add_collider(EntityID id, std::unique_ptr<Collider>&& collider, 
								  const Maths::Transform& offset) 
{
	ColliderComponent new_component;
	new_component.collider = std::move(collider);
	new_component.collider->apply_transform(offset);

	components.emplace(id, std::move(new_component));
}

const Collider* ColliderSystem::get_collider(EntityID id) const
{
	auto it = components.find(id);
	if (it == components.end())
	{
		return nullptr;
	}

	Collider* collider = it->second.collider.get();
	collider->set_temporary_transform(get_ecs().get_object(id).get_maths_transform());

	return collider;
}