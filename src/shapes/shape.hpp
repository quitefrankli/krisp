#pragma once

#include "vertex.hpp"

#include <glm/glm.hpp>


namespace Maths
{
	struct Ray;
}

class Shape
{
public:
	Shape() = default;
	~Shape() = default;
	Shape(const Shape& shape) = delete;
	Shape& operator=(const Shape& shape) = delete;
	Shape(Shape&& shape) noexcept = default;

	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	
	uint32_t get_num_unique_vertices() const { return static_cast<uint32_t>(vertices.size()); }
	uint32_t get_num_vertex_indices() const { return static_cast<uint32_t>(indices.size()); };
	void transform_vertices(const glm::mat4& transform);
	void generate_normals();

	// WARNING this is expensive
	void deduplicate_vertices();

	virtual bool check_collision(const Maths::Ray& ray);
};