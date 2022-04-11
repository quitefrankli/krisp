#include "objects.hpp"

#include "shapes/shapes.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <iostream>


ScaleGizmoObj::ScaleGizmoObj(const glm::vec3& original_axis) :
	original_axis(original_axis)
{
	shapes.reserve(2);
	auto& rod = shapes.emplace_back(Shapes::Cube{});
	auto& block = shapes.emplace_back(Shapes::Cube{});

	auto rod_transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.5f));
	rod_transform = glm::scale(rod_transform, glm::vec3(THICKNESS, THICKNESS, 1.0f));
	rod.transform_vertices(rod_transform);

	// to prevent z-fighting
	const float small_offset = 0.0001f;
	auto block_transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 1.0f - BLOCK_LENGTH * 0.5f + small_offset));
	block_transform = glm::scale(block_transform, glm::vec3(BLOCK_LENGTH));
	block.transform_vertices(block_transform);

	// warning this is expensive
	calculate_bounding_primitive<Maths::Sphere>();
}

void ScaleGizmoObj::point(const glm::vec3& start, const glm::vec3& end)
{
	const glm::vec3 v1 = Maths::forward_vec;
	const glm::vec3 v2 = glm::normalize(end - start);
	const glm::quat rot = Maths::RotationBetweenVectors(v1, v2);
	set_rotation(rot);
	set_position(start);
}

bool ScaleGizmoObj::check_collision(const Maths::Ray& ray)
{
	glm::vec3 axis = get_rotation() * Maths::forward_vec;

	const Maths::Sphere collision_sphere(
		get_position() + get_scale().z * axis * 0.5f,
		get_scale().z * 0.5f
	);
	if (!Maths::check_spherical_collision(ray, collision_sphere))
	{
		std::cout << "level 0 collision failed\n";
		return false;
	}

	// use cylinder collision for now
	auto normal = glm::normalize(glm::cross(ray.direction, axis));
	float dist = glm::distance(glm::dot(get_position(), normal) * normal, glm::dot(ray.origin, normal) * normal);
	return dist < THICKNESS;
}