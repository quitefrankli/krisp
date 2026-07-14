#include "mesh_bvh.hpp"

#include <algorithm>
#include <limits>

namespace
{
constexpr uint32_t LEAF_TRIANGLE_COUNT = 8;

AABB triangle_bounds(const std::vector<glm::vec3>& positions, const uint32_t* triangle)
{
	AABB bounds(positions[triangle[0]], positions[triangle[0]]);
	for (int i = 1; i < 3; ++i)
	{
		const auto& point = positions[triangle[i]];
		bounds.min_bound = glm::min(bounds.min_bound, point);
		bounds.max_bound = glm::max(bounds.max_bound, point);
	}
	return bounds;
}
}

MeshPickData::MeshPickData(std::vector<glm::vec3> positions, const std::vector<uint32_t>& indices) :
	positions(std::move(positions))
{
	if (!this->positions.empty())
	{
		bounds = AABB(this->positions.front(), this->positions.front());
		for (const auto& point : this->positions)
		{
			bounds.min_bound = glm::min(bounds.min_bound, point);
			bounds.max_bound = glm::max(bounds.max_bound, point);
		}
		has_local_bounds = true;
	}
	for (uint32_t offset = 0; offset + 2 < indices.size(); offset += 3)
	{
		if (indices[offset] < this->positions.size() &&
			indices[offset + 1] < this->positions.size() &&
			indices[offset + 2] < this->positions.size())
		{
			const glm::vec3 edge1 = this->positions[indices[offset + 1]] - this->positions[indices[offset]];
			const glm::vec3 edge2 = this->positions[indices[offset + 2]] - this->positions[indices[offset]];
			if (glm::length2(glm::cross(edge1, edge2)) > Maths::ACCEPTABLE_FLOATING_PT_DIFF)
				triangles.push_back({ { indices[offset], indices[offset + 1], indices[offset + 2] } });
		}
	}

	triangle_indices.resize(triangles.size());
	for (uint32_t i = 0; i < triangle_indices.size(); ++i)
		triangle_indices[i] = i;
	if (!triangle_indices.empty())
		build_node(0, static_cast<uint32_t>(triangle_indices.size()));
}

AABB MeshPickData::bounds_for_range(const uint32_t first, const uint32_t count) const
{
	const auto& first_triangle = triangles[triangle_indices[first]];
	AABB bounds = triangle_bounds(positions, first_triangle.vertices);
	for (uint32_t i = 1; i < count; ++i)
	{
		const auto& triangle = triangles[triangle_indices[first + i]];
		const AABB triangle_aabb = triangle_bounds(positions, triangle.vertices);
		bounds.min_bound = glm::min(bounds.min_bound, triangle_aabb.min_bound);
		bounds.max_bound = glm::max(bounds.max_bound, triangle_aabb.max_bound);
	}
	return bounds;
}

uint32_t MeshPickData::build_node(const uint32_t first, const uint32_t count)
{
	const uint32_t node_index = static_cast<uint32_t>(nodes.size());
	nodes.push_back({ .bounds = bounds_for_range(first, count), .first = first, .count = count });
	if (count <= LEAF_TRIANGLE_COUNT)
		return node_index;

	const glm::vec3 extent = nodes[node_index].bounds.max_bound - nodes[node_index].bounds.min_bound;
	const int axis = extent.y > extent.x && extent.y >= extent.z ? 1 : (extent.z > extent.x ? 2 : 0);
	const uint32_t middle = first + count / 2;
	std::nth_element(
		triangle_indices.begin() + first,
		triangle_indices.begin() + middle,
		triangle_indices.begin() + first + count,
		[this, axis](const uint32_t left, const uint32_t right)
		{
			const auto centroid = [this, axis](const uint32_t triangle_index)
			{
				const auto& triangle = triangles[triangle_index];
				return (positions[triangle.vertices[0]][axis] + positions[triangle.vertices[1]][axis] + positions[triangle.vertices[2]][axis]) / 3.0f;
			};
			return centroid(left) < centroid(right);
		});

	const uint32_t left_child = build_node(first, middle - first);
	const uint32_t right_child = build_node(middle, first + count - middle);
	nodes[node_index].first = left_child;
	nodes[node_index].second = right_child;
	nodes[node_index].count = 0;
	return node_index;
}
