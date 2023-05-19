#pragma once

#include "objects/object_id.hpp"
#include "collision/collider.hpp"
#include "maths.hpp"

#include <unordered_map>
#include <memory>


using EntityID = ObjectID;

struct ColliderComponent
{
	std::unique_ptr<Collider> collider;
};

class ECS;
class ColliderSystem
{
public:
	virtual ECS& get_ecs() = 0;
	virtual const ECS& get_ecs() const = 0;

	void add_collider(EntityID id, std::unique_ptr<Collider>&& collider);
	void add_collider(EntityID id, std::unique_ptr<Collider>&& collider, const Maths::Transform& offset);
	void remove_collider(EntityID id) { components.erase(id); }

	const Collider* get_collider(EntityID id) const;

protected:
	void remove_entity(EntityID id) { components.erase(id); }

private:
	std::unordered_map<EntityID, ColliderComponent> components;
};