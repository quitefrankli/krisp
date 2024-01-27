#pragma once

#include "identifications.hpp"
#include "collision/collider.hpp"
#include "common.hpp"
#include "maths.hpp"

#include <glm/vec3.hpp>

#include <unordered_set>


class HoverableSystem
{
public:
	virtual ECS& get_ecs() = 0;
	virtual const ECS& get_ecs() const = 0;

	void add_hoverable_entity(EntityID id);
	void remove_hoverable_entity(EntityID id) { hoverable_entities.erase(id); }

	DetectedEntityCollision check_any_entity_hovered(const Maths::Ray& ray) const;

protected:
	void remove_entity(EntityID id) { hoverable_entities.erase(id); }

private:
	std::unordered_set<EntityID> hoverable_entities;
};