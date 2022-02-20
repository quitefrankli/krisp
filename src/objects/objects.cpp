#include "objects.hpp"
#include "shapes/shapes.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <iostream>


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
	Shapes::Square left, right, front, back, top, bottom;
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
	auto calculate_vertex = [&M, &N](float m, float n)
	{
		m = m / (float)M * Maths::PI;
		n = n / (float)N * Maths::PI;
		Vertex vertex;
		vertex.pos = {
			sinf(2.0f * n) * sinf(m),
			cosf(m),
			cosf(2.0f * n) * sinf(m)
		};
		vertex.pos /= 2.0f;
		vertex.color = {
			sinf(m*2.0f)/2.0f+0.5f, 1.0f, sinf(n*2.0f)/2.0f+0.5f
		};
		return vertex;
	};

	// generate all vertices of sphere
	Shape shape;
	shape.vertices.reserve(M*N);
	shape.indices.reserve(M*N*8);

	for (int m = 0; m <= M; m++)
	{
		for (int n = 0; n < N; n++)
		{
			shape.vertices.push_back(calculate_vertex(m, n));
		}
	}
	
	auto get_index = [&](int m, int n)
	{
		return m*N + n;
	};

	// edge case first row
	for (int n = 0; n < N; n++)
	{
		const int m = 1;
		shape.indices.push_back(0);
		shape.indices.push_back(get_index(m, n));
		shape.indices.push_back(get_index(m, (n+1)%N));
	}
	for (int m = 1; m < M-1; m++)
	{
		for (int n = 0; n < N; n++)
		{
			shape.indices.push_back(get_index(m, n));
			shape.indices.push_back(get_index(m+1, n));
			// mod N here due to edge case at last column of a row
			shape.indices.push_back(get_index(m+1, (n+1)%N));
			shape.indices.push_back(get_index(m, n));
			shape.indices.push_back(get_index(m+1, (n+1)%N));
			shape.indices.push_back(get_index(m, (n+1)%N));
		}
	}
	// edge case last row
	for (int n = 0; n < N; n++)
	{
		const int m = M-1;
		shape.indices.push_back(get_index(m, n));
		shape.indices.push_back(shape.vertices.size()-1);
		shape.indices.push_back(get_index(m, (n+1)%N));
	}

	shape.generate_normals();
	shapes.push_back(std::move(shape));
}

HollowCylinder::HollowCylinder()
{
	const int M = 30;
	auto calculate_vec = [&M](float m, float y, float radius=1.0f, bool reverse=false)
	{
		m = m / (float)M * Maths::PI * 2.0f;
		Vertex vertex;
		vertex.pos = {
			sinf(m) * radius * 0.5f,
			y * 0.5f,
			cosf(m) * radius * 0.5f
		};
		if (reverse)
			vertex.color = { 0.0f, m/(float)M, 0.0f };
		else
			vertex.color = { m/(float)M, 0.0f, 0.0f };

		return vertex;
	};

	Shape shape;
	shape.vertices.reserve(1024);
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

	// make the bottom its origin
	auto vertex_transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.25f, 0.0f));
	for (auto& shape : shapes)
	{
		shape.transform_vertices(vertex_transform);
	}
}

static Shape generate_cylinder(const int nVertices)
{
	const glm::vec3 COLOR{ 1.0f, 1.0f, 0.0f };
	auto calculate_vec = [&nVertices, &COLOR](float m, float y)
	{
		m = m / (float)nVertices * Maths::PI * 2.0f;
		Vertex vertex;
		vertex.pos = {
			sinf(m) * 0.5f,
			y * 0.5f,
			cosf(m) * 0.5f
		};

		vertex.color = COLOR;

		return vertex;
	};

	Shape shape;
	shape.vertices.reserve(1024);
	Vertex top{ glm::vec3(0.0f, 0.5f, 0.0f), COLOR };
	Vertex bottom{ glm::vec3(0.0f, -0.5f, 0.0f), COLOR };
	for (int m = 0; m < nVertices; m++)
	{
		// side
		shape.vertices.push_back(calculate_vec(m, 1.0f));
		shape.vertices.push_back(calculate_vec(m, -1.0f));
		shape.vertices.push_back(calculate_vec(m+1, -1.0f));
		shape.vertices.push_back(calculate_vec(m, 1.0f));
		shape.vertices.push_back(calculate_vec(m+1, -1.0f));
		shape.vertices.push_back(calculate_vec(m+1, 1.0f));

		// top
		shape.vertices.push_back(calculate_vec(m, 1.0f));
		shape.vertices.push_back(calculate_vec(m+1, 1.0f));
		shape.vertices.push_back(top);

		//bottom
		shape.vertices.push_back(bottom);
		shape.vertices.push_back(calculate_vec(m+1, -1.0f));
		shape.vertices.push_back(calculate_vec(m, -1.0f));
	}
	
	return shape;
}

Cylinder::Cylinder()
{
	shapes.push_back(std::move(generate_cylinder(30)));

	// make the bottom its origin
	auto vertex_transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.5f, 0.0f));
	for (auto& shape : shapes)
	{
		shape.transform_vertices(vertex_transform);
	}
}

Arrow::Arrow()
{
	const int nVertices = 8;
	const glm::vec3 COLOR{ 1.0f, 1.0f, 0.0f };
	
	shapes.emplace_back(Shapes::Cylinder(nVertices, COLOR));
	shapes.emplace_back(Shapes::Cone(nVertices, COLOR));

	Shape& cylinder = shapes[0];
	Shape& cone = shapes[1];

	glm::quat quat = glm::angleAxis(Maths::PI/2.0f, glm::vec3(-1.0f, 0.0f, 0.0f));

	glm::mat4 cylinder_transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.4f, 0.0f));
	cylinder_transform = cylinder_transform * glm::scale(glm::mat4(1.0f), glm::vec3(RADIUS*2.0f, 0.8f, RADIUS*2.0f));
	cylinder_transform = glm::mat4_cast(quat) * cylinder_transform;
	cylinder.transform_vertices(cylinder_transform);

	glm::mat4 cone_transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.9f, 0.0f));
	cone_transform = cone_transform * glm::scale(glm::mat4(1.0f), glm::vec3(0.3f, 0.2f, 0.3f));
	cone_transform = glm::mat4_cast(quat) * cone_transform;
	cone.transform_vertices(cone_transform);

	// warning this is expensive
	calculate_bounding_primitive<Maths::Sphere>();
}

void Arrow::point(const glm::vec3& start, const glm::vec3& end)
{
	glm::vec3 v1(0.0f, 0.0f, -1.0f); // aka forward vector
	auto v2 = glm::normalize(end - start);
	glm::quat rot = Maths::RotationBetweenVectors(v1, v2);
	set_rotation(rot);
	set_position(start);

	auto scale = get_scale();
	scale.z = glm::distance(start, end);
	set_scale(scale);
}

bool Arrow::check_collision(Maths::Ray& ray)
{
	if (!Object::check_collision(ray))
	{
		std::cout << "level 0 collision failed\n";
		return false;
	}

	glm::vec3 axis = get_rotation() * Maths::forward_vec;
	auto normal = glm::normalize(glm::cross(ray.direction, axis));
	float dist = glm::distance(glm::dot(get_position(), normal) * normal, glm::dot(ray.origin, normal) * normal);
	return dist < RADIUS;
}