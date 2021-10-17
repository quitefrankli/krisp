#pragma once

#include "shapes.hpp"

#include <vector>


class GraphicsEngineObject;

class ObjectAbstract
{
protected:
	glm::mat4 transformation = glm::mat4(1.0f);
	glm::mat4 original_transformation = glm::mat4(1.0f);

public:
	ObjectAbstract();
	ObjectAbstract(uint64_t id);

	virtual glm::vec3 get_position() const { return transformation[3]; }
	virtual void set_position(const glm::vec3& pos) { transformation[3] = glm::vec4(pos, 1.0f); }
	virtual glm::mat4 get_transformation() const { return transformation; }
	virtual void set_transformation(const glm::mat4& transformation) { this->transformation = transformation; }
	virtual void apply_transformation(const glm::mat4& transformation);
	virtual glm::mat4 get_original_transformation() { return original_transformation; }
	virtual void set_original_transformation(glm::mat4 transformation) { original_transformation = transformation; }

	uint64_t get_id() const { return id; }
	// void set_id(uint64_t id) { this->id = id; } ;// we don't really want to set id ever
	void generate_new_id() { id = global_id++; }

private:
	static uint64_t global_id;
	uint64_t id;
};

class Object : public ObjectAbstract
{
protected:
	std::vector<Shape> shapes;

public:
	std::vector<std::vector<Vertex>>& get_vertex_sets();

private:
	std::vector<std::vector<Vertex>> cached_vertex_sets;
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