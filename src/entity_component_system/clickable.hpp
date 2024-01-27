#pragma once

#include "identifications.hpp"
#include "collision/collider.hpp"
#include "common.hpp"
#include "maths.hpp"

#include <glm/vec3.hpp>

#include <unordered_set>


class ClickableSystem
{
public:
	virtual ECS& get_ecs() = 0;
	virtual const ECS& get_ecs() const = 0;

	void add_clickable_entity(EntityID id);
	void remove_clickable_entity(EntityID id) { clickable_entities.erase(id); }

	DetectedEntityCollision check_any_entity_clicked(const Maths::Ray& ray) const;

protected:
	void remove_entity(EntityID id) { clickable_entities.erase(id); }

private:
	std::unordered_set<EntityID> clickable_entities;
};