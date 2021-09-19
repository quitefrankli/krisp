#pragma once

#include "vertex.hpp"

#include <glm/glm.hpp>

class Shape
{
protected:
	std::vector<Vertex> vertices;

public:
	Shape() {}
	~Shape() {}

	const std::vector<Vertex>& get_vertices() const { return vertices; }
	void transform_vertices(const glm::mat4& transform);
};

class Plane : public Shape
{
public:
	Plane();
	~Plane() {}
};

class Square : public Plane
{
};

class Triangle : public Shape
{
public:
	Triangle();
	~Triangle() {}
};

