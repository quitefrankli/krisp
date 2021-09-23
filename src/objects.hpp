#pragma once

#include "shapes.hpp"

#include <vector>

class Object
{
protected:
	glm::mat4 model = glm::mat4(1.0f);
	std::vector<Shape> shapes;

public:
	// Object();
	glm::vec3 get_pos() { return model[3]; }
	void set_pos(glm::vec3 pos) { model[3] = glm::vec4(pos, 1.0f); }
	std::vector<std::vector<Vertex>>& get_vertex_sets();
	glm::mat4 get_transformation() { return model; }
	glm::mat4 get_original_transformation() { return original_transformation; }
	void set_transformation(glm::mat4 transform) { model = transform; }
	void set_original_transformation(glm::mat4 transform) { original_transformation = transform; }
private:
	std::vector<std::vector<Vertex>> cached_vertex_sets;
	glm::mat4 original_transformation = glm::mat4(1.0f);
};

class Pyramid : public Object
{
public:
	Pyramid();
};

class Cube : public Object
{
public:
	Cube();
};