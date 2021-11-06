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

Sphere::Sphere()
{
	const int M = 30;
	const int N = 30;
	auto calculate_vec = [&M, &N](float m, float n)
	{
		m = m / (float)M * Maths::PI;
		n = n / (float)N * Maths::PI;
		Vertex vertex;
		vertex.pos = {
			sinf(2.0f * n) * sinf(m),
			cosf(m),
			cosf(2.0f * n) * sinf(m)
		};
		vertex.color = {
			m / (float)M, n / (float)N, 0.1f
		};
		return vertex;
	};

	std::vector<Vertex> vertices;
	vertices.reserve(1024);
	for (int m = 0; m < M+1; m++)
	{
		for (int n = 0; n < N; n++)
		{
			vertices.push_back(calculate_vec(m, n));
		}
	}
	Shape shape;
	shape.vertices.reserve(2048);
	for (int i = 0; i < M*N; i++)
	{
		if ((i+1)%N==0)
		{
			shape.vertices.push_back(vertices[i]);
			shape.vertices.push_back(vertices[i + N]);
			shape.vertices.push_back(vertices[i + 1]);
			shape.vertices.push_back(vertices[i]);
			shape.vertices.push_back(vertices[i + 1]);
			shape.vertices.push_back(vertices[i - N + 1]);
		} else 
		{
			shape.vertices.push_back(vertices[i]);
			shape.vertices.push_back(vertices[i + N]);
			shape.vertices.push_back(vertices[i + N + 1]);
			shape.vertices.push_back(vertices[i]);
			shape.vertices.push_back(vertices[i + N + 1]);
			shape.vertices.push_back(vertices[i + 1]);
		}
	}
	shapes.push_back(std::move(shape));
}

HollowCylinder::HollowCylinder()
{
	const int M = 10;
	auto calculate_vec = [&M](float m, float y, float radius=1.0f, bool reverse=false)
	{
		m = m / (float)M * Maths::PI * 2.0f;
		Vertex vertex;
		vertex.pos = {
			sinf(m) * radius,
			y,
			cosf(m) * radius
		};
		if (reverse)
			vertex.color = { 0.0f, m/(float)M, 0.0f };
		else
			vertex.color = { m/(float)M, 0.0f, 0.0f };

		return vertex;
	};

	Shape shape;
	shape.vertices.reserve(1024);
	Vertex top_vertex{
		glm::vec3(0.0f, 0.5f, 0.0f),
		glm::vec3(0.0f, 0.3f, 0.9f)
	};
	Vertex bottom_vertex{
		glm::vec3(0.0f, -0.5f, 0.0f),
		glm::vec3(0.0f, 1.0f, 0.2f)
	};
	for (int m = 0; m < M; m++)
	{
		// side
		shape.vertices.push_back(calculate_vec(m, 0.5f));
		shape.vertices.push_back(calculate_vec(m, -0.5f));
		shape.vertices.push_back(calculate_vec(m+1, -0.5f));
		shape.vertices.push_back(calculate_vec(m, 0.5f));
		shape.vertices.push_back(calculate_vec(m+1, -0.5f));
		shape.vertices.push_back(calculate_vec(m+1, 0.5f));

		// inside side
		shape.vertices.push_back(calculate_vec(m+1, 0.5f, 0.5f, true));
		shape.vertices.push_back(calculate_vec(m+1, -0.5f, 0.5f, true));
		shape.vertices.push_back(calculate_vec(m, 0.5f, 0.5f, true));
		shape.vertices.push_back(calculate_vec(m+1, -0.5f, 0.5f, true));
		shape.vertices.push_back(calculate_vec(m, -0.5f, 0.5f, true));
		shape.vertices.push_back(calculate_vec(m, 0.5f, 0.5f, true));

		// top
		shape.vertices.push_back(calculate_vec(m, 0.5f));
		shape.vertices.push_back(calculate_vec(m+1, 0.5f, 0.5f));
		shape.vertices.push_back(calculate_vec(m, 0.5f, 0.5f));
		shape.vertices.push_back(calculate_vec(m, 0.5f));
		shape.vertices.push_back(calculate_vec(m+1, 0.5f));
		shape.vertices.push_back(calculate_vec(m+1, 0.5f, 0.5f));

		//bottom
		shape.vertices.push_back(calculate_vec(m+1, -0.5f, 0.5f));
		shape.vertices.push_back(calculate_vec(m+1, -0.5f));
		shape.vertices.push_back(calculate_vec(m, -0.5f));
		shape.vertices.push_back(calculate_vec(m, -0.5f, 0.5f));
		shape.vertices.push_back(calculate_vec(m+1, -0.5f, 0.5f));
		shape.vertices.push_back(calculate_vec(m, -0.5f));
	}
	shapes.push_back(std::move(shape));
}