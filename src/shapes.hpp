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
	const std::vector<Vertex>& get_vertices() const { return vertices; }
	void transform_vertices(const glm::mat4& transform);
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

