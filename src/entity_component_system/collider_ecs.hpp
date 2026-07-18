#pragma once

#include "identifications.hpp"
#include "collision/collider.hpp"
#include "maths.hpp"
#include "common.hpp"

#include <unordered_map>
#include <memory>
#include <optional>


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
	void add_mesh_collider(EntityID id);
	void remove_collider(EntityID id) { components.erase(id); }

	const Collider* get_collider(EntityID id) const;
	// Returns the nearest registered collider hit by the ray. The optional entity
	// filter is used by character controllers so they do not hit themselves.
	DetectedEntityCollision raycast(const Maths::Ray& ray, std::optional<EntityID> ignored = std::nullopt) const;
	const std::unordered_map<EntityID, ColliderComponent>& get_all_colliders() const { return components; }
	void serialize(Serializer& out) const;
	void deserialize(const Deserializer& in);

protected:
	void remove_entity(EntityID id) { components.erase(id); }

private:
	std::unordered_map<EntityID, ColliderComponent> components;
};
