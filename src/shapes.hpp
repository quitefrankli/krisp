#pragma once

#include "vertex.hpp"

class Shape
{

protected:
	std::vector<Vertex> vertices;

public:
	const std::vector<Vertex>& get_vertices() { return vertices; }

	Shape() {}
	~Shape() {}
};

class Plane : public Shape
{
public:
	Plane();
	~Plane() {}
};

class Triangle : public Shape
{
public:
	Triangle();
	~Triangle() {}
};

