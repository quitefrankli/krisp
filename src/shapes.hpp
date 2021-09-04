#pragma once

#include "vertex.hpp"

class Shape
{
private:
	glm::mat4 model; // TODO: when we have object class, move this to the object instead of shape

protected:
	std::vector<Vertex> vertices;

public:
	virtual glm::vec3 get_pos();
	virtual void set_pos(glm::vec3 pos);

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

