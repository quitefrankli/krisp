#include "bounding_box.hpp"

#include "objects/object.hpp"

#include <limits>


AABB::AABB() = default;

AABB::AABB(const glm::vec3& min_bound, const glm::vec3& max_bound) :
	min_bound(min_bound), max_bound(max_bound)
{
}

void AABB::min_max(const AABB& other) 
{
	min_bound.x = std::min(min_bound.x, other.min_bound.x);
	min_bound.y = std::min(min_bound.y, other.min_bound.y);
	min_bound.z = std::min(min_bound.z, other.min_bound.z);
	max_bound.x = std::max(max_bound.x, other.max_bound.x);
	max_bound.y = std::max(max_bound.y, other.max_bound.y);
	max_bound.z = std::max(max_bound.z, other.max_bound.z);
}

bool AABB::check_collision(const Maths::Ray& ray) const
{
	glm::vec3 intersection;
	return check_collision(ray, intersection);
}

bool AABB::check_collision(const Maths::Ray& ray, glm::vec3& intersection) const
{
	float near_t = -std::numeric_limits<float>::infinity();
	float far_t = std::numeric_limits<float>::infinity();
	for (int axis = 0; axis < 3; ++axis)
	{
		const float direction = ray.direction[axis];
		const float origin = ray.origin[axis];
		if (Maths::absf(direction) <= std::numeric_limits<float>::epsilon())
		{
			if (origin < min_bound[axis] || origin > max_bound[axis])
				return false;
			continue;
		}

		float t0 = (min_bound[axis] - origin) / direction;
		float t1 = (max_bound[axis] - origin) / direction;
		if (t0 > t1)
			std::swap(t0, t1);
		near_t = std::max(near_t, t0);
		far_t = std::min(far_t, t1);
		if (near_t > far_t)
			return false;
	}

	if (far_t < 0.0f)
		return false;
	intersection = ray.origin + ray.direction * std::max(near_t, 0.0f);
	return true;
}
