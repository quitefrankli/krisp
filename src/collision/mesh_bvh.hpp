#pragma once

#include "collision/bounding_box.hpp"

#include <glm/vec3.hpp>

#include <cstdint>
#include <vector>

struct MeshBvhNode
{
	AABB bounds;
	uint32_t first = 0;
	uint32_t second = 0;
	uint32_t count = 0;

	bool is_leaf() const { return count != 0; }
};

struct MeshPickTriangle
{
	uint32_t vertices[3];
};

// Immutable local-space picking data associated with a Mesh.
class MeshPickData
{
public:
	MeshPickData() = default;
	MeshPickData(std::vector<glm::vec3> positions, const std::vector<uint32_t>& indices);

	bool has_bounds() const { return has_local_bounds; }
	bool has_triangles() const { return !triangles.empty(); }
	const AABB& get_bounds() const { return bounds; }
	const std::vector<glm::vec3>& get_positions() const { return positions; }
	const std::vector<MeshPickTriangle>& get_triangles() const { return triangles; }
	const std::vector<uint32_t>& get_triangle_indices() const { return triangle_indices; }
	const std::vector<MeshBvhNode>& get_nodes() const { return nodes; }

private:
	uint32_t build_node(uint32_t first, uint32_t count);
	AABB bounds_for_range(uint32_t first, uint32_t count) const;

	std::vector<glm::vec3> positions;
	AABB bounds;
	bool has_local_bounds = false;
	std::vector<MeshPickTriangle> triangles;
	std::vector<uint32_t> triangle_indices;
	std::vector<MeshBvhNode> nodes;
};
