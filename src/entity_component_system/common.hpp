#pragma once

#include "identifications.hpp"

#include <glm/vec3.hpp>

#include <unordered_set>


using EntityID = ObjectID;;

struct DetectedEntityCollision
{
	bool bCollided = false;
	EntityID id = EntityID(0);
	glm::vec3 intersection;
};

class ECS;
