#pragma once

#include "vertex.hpp"

#include <glm/glm.hpp>

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
	
	uint32_t get_num_unique_vertices() const { return vertices.size(); }
	uint32_t get_num_vertex_indices() const { return indices.size(); };
	void transform_vertices(const glm::mat4& transform);
	void generate_normals();
};

class Square : public Shape
{
public:
	Square();
	Square(Square&& square) noexcept = default;
};

class Triangle : public Shape
{
public:
	Triangle();
	Triangle(Triangle&& triangle) noexcept = default;
};

