#include "bounding_box.hpp"

#include "objects/object.hpp"

#include <glm/gtx/component_wise.hpp>
#include <glm/gtx/string_cast.hpp>

#include <iostream>


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

// from
// https://medium.com/@bromanz/another-view-on-the-classic-ray-aabb-intersection-algorithm-for-bvh-traversal-41125138b525
bool AABB::check_collision(const Maths::Ray& ray) const
{
	const auto invRaydir = 1.0f/ray.direction;
	const glm::vec3 t0 = (min_bound - ray.origin) * invRaydir;
	std::cout<<glm::to_string(t0)<<'\n';
	const glm::vec3 t1 = (max_bound - ray.origin) * invRaydir;
	const glm::vec3 tmin(std::min(t0.x, t1.x),
						 std::min(t0.y, t1.y),
						 std::min(t0.z, t1.z));
	const glm::vec3 tmax(std::max(t0.x, t1.x),
						 std::max(t0.y, t1.y),
						 std::max(t0.z, t1.z));
	const float minCompOfMaxSet = glm::compMin(tmax);
	const float maxCompOfMinSet = glm::compMax(tmin);
  	return maxCompOfMinSet <= minCompOfMaxSet && minCompOfMaxSet > 0.0f;
}

bool AABB::check_collision(const Maths::Ray& ray, glm::vec3& intersection) const
{
	const auto invRaydir = 1.0f/ray.direction;
	const glm::vec3 t0 = (min_bound - ray.origin) * invRaydir;
	const glm::vec3 t1 = (max_bound - ray.origin) * invRaydir;
	const glm::vec3 tmin(std::min(t0.x, t1.x),
						 std::min(t0.y, t1.y),
						 std::min(t0.z, t1.z));
	const glm::vec3 tmax(std::max(t0.x, t1.x),
						 std::max(t0.y, t1.y),
						 std::max(t0.z, t1.z));
	const float minCompOfMaxSet = glm::compMin(tmax);
	const float maxCompOfMinSet = glm::compMax(tmin);
	if (maxCompOfMinSet > minCompOfMaxSet || minCompOfMaxSet < 0.0f)
	{
		return false;
	}

	intersection = ray.origin + ray.direction * maxCompOfMinSet;
	return true;
}
