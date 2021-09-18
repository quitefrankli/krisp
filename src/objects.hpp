#pragma once

#include "shapes.hpp"

#include <vector>

class Object
{
protected:
	glm::mat4 model;
	std::vector<Shape> shapes;

public:
	// Object();
	glm::vec3 get_pos() { return model[3]; }
	void set_pos(glm::vec3 pos) { model[3] = glm::vec4(pos, 1.0f); }
};

class Pyramid : public Object
{
public:
	Pyramid();
};