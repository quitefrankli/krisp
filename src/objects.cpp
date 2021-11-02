#include "objects.hpp"
#include "maths.hpp"
#include "resource_loader.hpp"

#include <glm/ext.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include <iostream>


uint64_t ObjectAbstract::global_id = 0;

ObjectAbstract::ObjectAbstract()
{
	generate_new_id();
}

ObjectAbstract::ObjectAbstract(uint64_t id)
{
	this->id = id;
}

Object::Object(ResourceLoader& loader, std::string mesh, std::string texture)
{
	loader.load_mesh(*this, mesh);
	this->texture = texture;
}

Object::Object(std::string texture_) : 
	texture(texture_)
{
}

std::vector<std::vector<Vertex>>& Object::get_vertex_sets()
{
	if (is_vertex_sets_old)
	{
		is_vertex_sets_old = false;
		for (auto& shape : shapes)
		{
			cached_vertex_sets.emplace_back(shape.get_vertices());
		}
	}

	return cached_vertex_sets;
}

void Object::set_position(glm::vec3& position)
{
	is_transform_old = true;
	this->position = position;
}

void Object::set_scale(glm::vec3& scale)
{
	is_transform_old = true;
	this->scale = scale;
}

void Object::set_rotation(glm::quat& rotation)
{
	is_transform_old = true;
	this->orientation = rotation;
}

glm::mat4 Object::get_transform()
{
	if (is_transform_old)
	{
		is_transform_old = false;

		cached_transform = glm::mat4_cast(glm::normalize(orientation)) * glm::scale(scale);
		cached_transform[3] = glm::vec4(position, 1.0f);
	}

	return cached_transform;
}

void Object::set_transform(glm::mat4& transform)
{
	is_transform_old = false;
	cached_transform = transform;

	auto get_vec3_len = [](glm::vec4& vec4)
	{
		return std::sqrtf(vec4[0] * vec4[0] + vec4[1] * vec4[1] + vec4[2] * vec4[2]);
	};

	// https://math.stackexchange.com/questions/237369/given-this-transformation-matrix-how-do-i-decompose-it-into-translation-rotati/417813
	orientation = glm::normalize(glm::quat_cast(transform));
	scale[0] = get_vec3_len(transform[0]);
	scale[1] = get_vec3_len(transform[1]);
	scale[2] = get_vec3_len(transform[2]);
	position = transform[3];
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

	shapes.push_back(std::move(left));
	shapes.push_back(std::move(right));
	shapes.push_back(std::move(back));
	shapes.push_back(std::move(bottom));
}

Cube::Cube()
{
	init();
}

Cube::Cube(std::string texture) :
	Object(texture)
{
	init();
}

void Cube::init()
{
	Square left, right, front, back, top, bottom;
	glm::mat4 idt(1.0f);
	glm::mat4 transform;

	transform = idt;
	// for transformation relative to world apply right->left, for local transformation it's left->right
	transform = glm::translate(transform, glm::vec3(-0.5f, 0.0f, 0.0f));
	transform = glm::rotate(transform, -Maths::deg2rad(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	left.transform_vertices(transform);

	transform = idt;
	transform = glm::translate(transform, glm::vec3(0.5f, 0.0f, 0.0f));
	transform = glm::rotate(transform, Maths::deg2rad(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	right.transform_vertices(transform);

	transform = idt;
	transform = glm::translate(transform, glm::vec3(0.0f, 0.0f, 0.5f));
	front.transform_vertices(transform);

	transform = idt;
	transform = glm::translate(transform, glm::vec3(0.0f, 0.0f, -0.5f));
	transform = glm::rotate(transform, Maths::deg2rad(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	back.transform_vertices(transform);

	transform = idt;
	transform = glm::translate(transform, glm::vec3(0.0f, 0.5f, 0.0f));
	transform = glm::rotate(transform, Maths::deg2rad(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	top.transform_vertices(transform);

	transform = idt;
	transform = glm::translate(transform, glm::vec3(0.0f, -0.5f, 0.0f));
	transform = glm::rotate(transform, Maths::deg2rad(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	bottom.transform_vertices(transform);

	shapes.push_back(std::move(left));
	shapes.push_back(std::move(right));
	shapes.push_back(std::move(front));
	shapes.push_back(std::move(back));
	shapes.push_back(std::move(top));
	shapes.push_back(std::move(bottom));
}