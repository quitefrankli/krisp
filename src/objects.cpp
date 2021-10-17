#include "objects.hpp"
#include "maths.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <iostream>


uint64_t ObjectAbstract::global_id = 0;

ObjectAbstract::ObjectAbstract()
{
	generate_new_id();
}

void ObjectAbstract::apply_transformation(const glm::mat4& transformation)
{
	this->transformation = transformation * (this->transformation);
	// this->transformation = (this->transformation) * transformation;
}

std::vector<std::vector<Vertex>>& Object::get_vertex_sets()
{
	if (!cached_vertex_sets.empty())
	{
		return cached_vertex_sets;
	}

	for (auto& shape : shapes)
	{
		cached_vertex_sets.emplace_back(shape.get_vertices());
	}

	return cached_vertex_sets;
}

Pyramid::Pyramid()
{
	Triangle left, right, back, bottom;
	{
		glm::mat4 transform(1.0f);
		transform = glm::translate(transform, glm::vec3(-0.5f, 0.0f, 0.0f));
		transform = glm::rotate(transform, Maths::deg2rad(90.0f)*2.0f, glm::vec3(0.0f, 1.0f, 0.0f));
		transform = glm::rotate(transform, Maths::deg2rad(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
		left.transform_vertices(transform);
	}
	{
		glm::mat4 transform(1.0f);
		transform = glm::translate(transform, glm::vec3(0.5f, 0.0f, 0.0f));
		transform = glm::rotate(transform, Maths::deg2rad(-90.0f)*2.0f, glm::vec3(0.0f, 1.0f, 0.0f));
		transform = glm::rotate(transform, Maths::deg2rad(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
		right.transform_vertices(transform);
	}
	{
		glm::mat4 transform(1.0f);
		transform = glm::translate(transform, glm::vec3(0.0f, 0.0f, 0.5f));
		back.transform_vertices(transform);
	}
	{
		glm::mat4 transform(1.0f);
		transform = glm::translate(transform, glm::vec3(0.0f, -0.5f, 0.0f));
		transform = glm::rotate(transform, Maths::deg2rad(-90.0f)*2.0f, glm::vec3(1.0f, 0.0f, 0.0f));
		bottom.transform_vertices(transform);
	}
	shapes = std::vector<Shape>{ left, right, back, bottom };
}

Cube::Cube()
{
	Square left, right, front, back, top, bottom;
	glm::mat4 idt(1.0f);
	glm::mat4 transform;

	transform = idt;
	// transform = glm::scale(transform, glm::vec3(0.5f));
	transform = glm::rotate(transform, -Maths::deg2rad(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	transform = glm::translate(idt, glm::vec3(-0.5f, 0.0f, 0.0f)) * transform;
	// transform = glm::rotate(transform, Maths::deg2rad(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	left.transform_vertices(transform);

	transform = idt;
	// transform = glm::scale(transform, glm::vec3(0.5f));
	transform = glm::translate(transform, glm::vec3(0.5f, 0.0f, 0.0f));
	transform = glm::rotate(transform, Maths::deg2rad(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	// transform = glm::rotate(transform, Maths::deg2rad(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	right.transform_vertices(transform);

	transform = idt;
	// transform = glm::scale(transform, glm::vec3(0.5f));
	transform = glm::translate(transform, glm::vec3(0.0f, 0.0f, 0.5f));
	front.transform_vertices(transform);

	transform = idt;
	// transform = glm::scale(transform, glm::vec3(0.5f));
	transform = glm::translate(transform, glm::vec3(0.0f, 0.0f, -0.5f));
	transform = glm::rotate(transform, Maths::deg2rad(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	back.transform_vertices(transform);

	transform = idt;
	// transform = glm::scale(transform, glm::vec3(0.5f));
	transform = glm::translate(transform, glm::vec3(0.0f, 0.5f, 0.0f));
	transform = glm::rotate(transform, -Maths::deg2rad(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	top.transform_vertices(transform);

	transform = idt;
	// transform = glm::scale(transform, glm::vec3(0.5f));
	transform = glm::translate(transform, glm::vec3(0.0f, -0.5f, 0.0f));
	transform = glm::rotate(transform, Maths::deg2rad(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	bottom.transform_vertices(transform);

	shapes = std::vector<Shape>{ left, right, front, back, top, bottom };
}

