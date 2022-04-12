#include "bounding_box.hpp"

#include "shapes/shape.hpp"
#include "objects/object.hpp"


AABB::AABB() = default;

AABB::AABB(const Shape& shape)
{
	min_bound = glm::vec3(std::numeric_limits<float>::max());
	max_bound = glm::vec3(-std::numeric_limits<float>::max());
	for (const auto& vertex : shape.vertices)
	{
		min_bound.x = std::min(min_bound.x, vertex.pos.x);
		min_bound.y = std::min(min_bound.y, vertex.pos.y);
		min_bound.z = std::min(min_bound.z, vertex.pos.z);
		max_bound.x = std::max(max_bound.x, vertex.pos.x);
		max_bound.y = std::max(max_bound.y, vertex.pos.y);
		max_bound.z = std::max(max_bound.z, vertex.pos.z);
	}
}

AABB::AABB(const std::vector<Shape>& shapes)
{
	min_bound = glm::vec3(std::numeric_limits<float>::max());
	max_bound = glm::vec3(-std::numeric_limits<float>::max());
	for (const auto& shape : shapes)
	{
		for (const auto& vertex : shape.vertices)
		{
			min_bound.x = std::min(min_bound.x, vertex.pos.x);
			min_bound.y = std::min(min_bound.y, vertex.pos.y);
			min_bound.z = std::min(min_bound.z, vertex.pos.z);
			max_bound.x = std::max(max_bound.x, vertex.pos.x);
			max_bound.y = std::max(max_bound.y, vertex.pos.y);
			max_bound.z = std::max(max_bound.z, vertex.pos.z);
		}
	}
}

AABB::AABB(const Object& object) : AABB(object.get_shapes()) {}