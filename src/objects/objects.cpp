#include "objects.hpp"
#include "shapes/shapes.hpp"
#include "shapes/shape_factory.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <iostream>


Cube::Cube()
{
	shapes.emplace_back(Shapes::Cube{});
}

Sphere::Sphere() : IClickable((Object&)(*this))
{
	shapes.emplace_back(ShapeFactory::generate_sphere(ShapeFactory::GenerationMethod::UV_SPHERE, 128));
}

bool Sphere::check_collision(const Maths::Ray& ray, glm::vec3& intersection) const
{
	const auto x = Maths::ray_sphere_collision(Maths::Sphere(get_position(), get_radius()), ray);
	if (x)
	{
		intersection = x.value();
		return true;
	} else 
	{
		return false;
	}
}

HollowCylinder::HollowCylinder()
{
	const int M = 30;
	auto calculate_vec = [&M](float m, float y, float radius=1.0f, bool reverse=false)
	{
		m = m / (float)M * Maths::PI * 2.0f;
		SDS::Vertex vertex;
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
		SDS::Vertex vertex;
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
	SDS::Vertex top{ glm::vec3(0.0f, 0.5f, 0.0f), COLOR };
	SDS::Vertex bottom{ glm::vec3(0.0f, -0.5f, 0.0f), COLOR };
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

	glm::quat quat = glm::angleAxis(Maths::PI/2.0f, glm::vec3(1.0f, 0.0f, 0.0f));

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
	const auto& v1 = Maths::forward_vec;
	const auto v2 = glm::normalize(end - start);
	const glm::quat rot = Maths::RotationBetweenVectors(v1, v2);
	set_rotation(rot);
	set_position(start);

	auto scale = get_scale();
	scale.z = glm::distance(start, end);
	set_scale(scale);
}

bool Arrow::check_collision(const Maths::Ray& ray)
{
	const glm::vec3 axis = get_rotation() * Maths::forward_vec;
	const Maths::Sphere collision_sphere(
		get_position() + get_scale().z * axis * 0.5f,
		get_scale().z * 0.5f
	);
	if (!Maths::check_spherical_collision(ray, collision_sphere))
	{
		std::cout << "level 0 collision failed\n";
		return false;
	}

	// the cross product of Rd and Ad gives the normal that will contain the
	// segment with the shortest distance assuming there is a collision
	const glm::vec3 normal = glm::normalize(glm::cross(ray.direction, axis));
	// projecting Ro and Ao onto said normal tells us whether or not given an 2 infinite rays
	// the shortest distance between any 2 points on the two.
	float dist = std::fabsf(glm::dot(get_position(), normal) - glm::dot(ray.origin, normal));
	// a cylinder is just a ray with a radius, so if the shortest possible distance
	// is greater than the radius of the cylinder then there is no intersection
	return dist < RADIUS;
}

bool Arrow::check_collision(const Maths::Ray& ray, glm::vec3& intersection) const
{
	// TODO: clean this up

	const glm::vec3 axis = get_rotation() * Maths::forward_vec;
	const Maths::Sphere collision_sphere(
		get_position() + get_scale().z * axis * 0.5f,
		get_scale().z * 0.5f
	);
	if (!Maths::check_spherical_collision(ray, collision_sphere))
	{
		std::cout << "level 0 collision failed\n";
		return false;
	}

	// the cross product of Rd and Ad gives the normal that will contain the
	// segment with the shortest distance assuming there is a collision
	const glm::vec3 normal = glm::normalize(glm::cross(ray.direction, axis));
	// projecting Ro and Ao onto said normal tells us whether or not given an 2 infinite rays
	// the shortest distance between any 2 points on the two.
	const float dist = std::fabsf(glm::dot(get_position(), normal) - glm::dot(ray.origin, normal));
	// a cylinder is just a ray with a radius, so if the shortest possible distance
	// is greater than the radius of the cylinder then there is no intersection
	const float radius = RADIUS * get_scale().x;
	if (dist > radius)
	{
		return false;
	}

	// Y = vector perpendicular to normal and arrow direction
	const glm::vec3 Y = glm::normalize(glm::cross(axis, normal));
	const float Ad_Y = glm::dot(axis, Y);				// Ad . Y
	const float Ad_Ad = glm::dot(axis, axis);			// Ad . Ad
	const glm::vec3 Ao_Ro = get_position() - ray.origin;	// Ao - Ro

	const float numerator = Ad_Y * glm::dot(Ao_Ro, axis) + Ad_Ad * glm::dot(-Ao_Ro, Y);
	const float denominator = Ad_Y * glm::dot(ray.direction, axis) - Ad_Ad * glm::dot(ray.direction, Y);
	const float t = numerator / denominator; // P = Ro + tRd
	intersection = ray.origin + t * ray.direction;

	// idk why the above intersection is wrong although I have theory,
	// anyways the below code is a quick hack to get it to work

	const float rr = radius * radius;
	const float xx = dist * dist;
	if (rr < xx)
	{
		std::cerr << "Arrow::check_collision(Ray, Intersection): no quadratic solution!\n";
		return false;
	}
	
	const float y = std::sqrtf(rr - xx);
	const glm::vec3 pA = intersection - y*Y;
	const glm::vec3 pB = intersection + y*Y;

	intersection = glm::dot(pA, ray.direction) < glm::dot(pB, ray.direction) ? pA : pB;

	return true;
}

Arc::Arc()
{
	// generated via 2 concentric circles
	const float thickness = 0.02f; // 3d objects require a little thickness
	const int N = 10; // num points
	const float inc = Maths::PI/2.0f/(float)N;

	// generate all vertices
	Shape arc;
	arc.vertices.reserve(N * 2);
	auto gen_vertex = [&](const float radius, const float y, const int i)
	{
		arc.vertices.emplace_back(SDS::Vertex{
			radius * glm::vec3(sinf(inc * i), y, cosf(inc * i)),
			glm::vec3((float)i/(float)N, 1.0f, 1.0f)});
	};
	for (int i = 0; i <= N; i++)
		gen_vertex(outer_radius, thickness*0.5f, i);
	for (int i = 0; i <= N; i++)
		gen_vertex(inner_radius, thickness*0.5f, i);
	for (int i = 0; i <= N; i++)
		gen_vertex(outer_radius, -thickness*0.5f, i);
	for (int i = 0; i <= N; i++)
		gen_vertex(inner_radius, -thickness*0.5f, i);

	const int outer_top_offset = 0;
	const int inner_top_offset = N+1;
	const int outer_bot_offset = N+N+2;
	const int inner_bot_offset = N+N+N+3;
	for (int i = 0; i < N; i++)
	{
		// top
		arc.indices.push_back(outer_top_offset + i);
		arc.indices.push_back(inner_top_offset + i);
		arc.indices.push_back(outer_top_offset + i + 1);
		arc.indices.push_back(outer_top_offset + i + 1);
		arc.indices.push_back(inner_top_offset + i);
		arc.indices.push_back(inner_top_offset + i + 1);

		// bottom
		arc.indices.push_back(outer_bot_offset + i);
		arc.indices.push_back(outer_bot_offset + i + 1);
		arc.indices.push_back(inner_bot_offset + i);
		arc.indices.push_back(inner_bot_offset + i);
		arc.indices.push_back(outer_bot_offset + i + 1);
		arc.indices.push_back(inner_bot_offset + i + 1);

		// we don't actually need the sides given a thin enough thickness and single sided shading
	}

	// let default plane normal be the forward axis
	arc.transform_vertices(glm::angleAxis(-Maths::deg2rad(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)));

	arc.generate_normals();
	shapes.push_back(std::move(arc));
}

bool Arc::check_collision(const Maths::Ray& ray)
{
	glm::vec3 intersection;
	return check_collision(ray, intersection);
}

bool Arc::check_collision(const Maths::Ray& ray, glm::vec3& intersection) const
{
	// imagine an arc as a plane, the default has a normal = upvector
	const Maths::Plane plane(get_position(), glm::normalize(get_rotation() * Maths::forward_vec));

	// first check if there is even an intersection
	if (!Maths::check_ray_plane_intersection(ray, plane))
		return false;

	intersection = Maths::ray_plane_intersection(ray, plane);
	const float dist = glm::distance(intersection, plane.offset);
	if (dist > outer_radius || dist < inner_radius)
	{
		// not on arc
		return false;
	}

	// this might be a tad inefficient but will do for now
	// but it essentially unrotates the mathematical representation of the arc
	// and checks if P lies within the arc
	glm::vec3 origP = glm::inverse(get_rotation()) * (intersection - plane.offset);
	return origP.x > 0 && origP.y > 0;
}