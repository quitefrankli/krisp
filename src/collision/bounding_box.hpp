#pragma once

#include <glm/vec3.hpp>

#include <vector>


class Shape;
class Object;

struct AABB
{
	AABB();
	AABB(const Shape& shape);
	AABB(const std::vector<Shape>& shapes);
	AABB(const Object& object);

	glm::vec3 min_bound;
	glm::vec3 max_bound;
};