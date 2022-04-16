#pragma once

#include "maths.hpp"

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
	AABB(const glm::vec3& min_bound, const glm::vec3& max_bound);

	glm::vec3 min_bound;
	glm::vec3 max_bound;

	bool check_collision(const Maths::Ray& ray) const;
	bool check_collision(const Maths::Ray& ray, glm::vec3& intersection) const;

	AABB operator+(const glm::vec3& vec) const
	{
		return AABB(min_bound+vec, max_bound+vec);
	}
	AABB operator-(const glm::vec3& vec) const
	{
		return AABB(min_bound-vec, max_bound-vec);
	}
	AABB& operator+=(const glm::vec3& vec)
	{
		min_bound += vec;
		max_bound += vec;
		return *this;
	}
	AABB& operator-=(const glm::vec3& vec)
	{
		min_bound -= vec;
		max_bound -= vec;
		return *this;
	}
};