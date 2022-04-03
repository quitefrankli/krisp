#include "objects.hpp"

#include "shapes/shapes.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <iostream>


ScaleGizmoObj::ScaleGizmoObj()
{
	shapes.reserve(2);
	auto& rod = shapes.emplace_back(Shapes::Cube{});
	auto& block = shapes.emplace_back(Shapes::Cube{});

	auto rod_transform = glm::translate(glm::mat4(1.0f), -glm::vec3(0.0f, 0.0f, 0.5f));
	rod_transform = glm::scale(rod_transform, glm::vec3(THICKNESS, THICKNESS, 1.0f));
	rod.transform_vertices(rod_transform);

	// to prevent z-fighting
	const float small_offset = 0.0001f;
	auto block_transform = glm::translate(glm::mat4(1.0f), -glm::vec3(0.0f, 0.0f, 1.0f - BLOCK_LENGTH * 0.5f + small_offset));
	block_transform = glm::scale(block_transform, glm::vec3(BLOCK_LENGTH));
	block.transform_vertices(block_transform);

	// warning this is expensive
	calculate_bounding_primitive<Maths::Sphere>();
}

void ScaleGizmoObj::point(const glm::vec3& start, const glm::vec3& end)
{
	glm::vec3 v1(0.0f, 0.0f, -1.0f); // aka forward vector
	auto v2 = glm::normalize(end - start);
	glm::quat rot = Maths::RotationBetweenVectors(v1, v2);
	set_rotation(rot);
	set_position(start);
}

bool ScaleGizmoObj::check_collision(const Maths::Ray& ray)
{
	return false; // TODO
	// if (!Object::check_collision(ray))
	// {
	// 	std::cout << "level 0 collision failed\n";
	// 	return false;
	// }

	// glm::vec3 axis = get_rotation() * Maths::forward_vec;
	// auto normal = glm::normalize(glm::cross(ray.direction, axis));
	// float dist = glm::distance(glm::dot(get_position(), normal) * normal, glm::dot(ray.origin, normal) * normal);
	// return dist < RADIUS;
}