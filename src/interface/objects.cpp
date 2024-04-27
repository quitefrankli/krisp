#include "objects.hpp"
#include "renderable/mesh_factory.hpp"
#include "renderable/mesh_maths.hpp"
#include "entity_component_system/material_system.hpp"
#include "entity_component_system/mesh_system.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <ranges>
#include <algorithm>


ScaleGizmoObj::ScaleGizmoObj(const glm::vec3& original_axis) :
	original_axis(original_axis)
{
	MeshPtr rod_ptr = MeshFactory::cube();
	MeshPtr block_ptr = MeshFactory::cube();
	auto& rod = static_cast<ColorMesh&>(*rod_ptr);
	auto& block = static_cast<ColorMesh&>(*block_ptr);
	auto rod_vertices = rod.get_vertices();
	auto block_vertices = block.get_vertices();
	auto rod_indices = rod.get_indices();
	auto block_indices = block.get_indices();

	auto rod_transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.5f));
	rod_transform = glm::scale(rod_transform, glm::vec3(THICKNESS, THICKNESS, 1.0f));
	transform_vertices(rod_vertices, rod_transform);

	// to prevent z-fighting
	const float small_offset = 0.0001f;
	auto block_transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 1.0f - BLOCK_LENGTH * 0.5f + small_offset));
	block_transform = glm::scale(block_transform, glm::vec3(BLOCK_LENGTH));
	transform_vertices(block_vertices, block_transform);

	concatenate_vertices(rod_vertices, rod_indices, block_vertices, block_indices);

	const auto mesh_id = MeshSystem::add(std::make_unique<ColorMesh>(std::move(rod_vertices), std::move(rod_indices)));

	renderables = { Renderable::make_default(mesh_id) };
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
		return false;
	}

	// use cylinder collision for now
	auto normal = glm::normalize(glm::cross(ray.direction, axis));
	float dist = glm::distance(glm::dot(get_position(), normal) * normal, glm::dot(ray.origin, normal) * normal);
	return dist < THICKNESS;
}