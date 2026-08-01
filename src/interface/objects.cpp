#include "objects.hpp"
#include "renderable/mesh_factory.hpp"
#include "renderable/mesh_maths.hpp"
#include "entity_component_system/material_system.hpp"
#include "entity_component_system/mesh_system.hpp"
#include "entity_component_system/ecs.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <ranges>
#include <algorithm>


ScaleGizmoObj::ScaleGizmoObj(const glm::vec3& original_axis) :
	original_axis(original_axis)
{}

Renderable ScaleGizmoObj::make_renderable(ECS& ecs)
{
	MeshPtr rod_ptr = MeshFactory::cylinder();
	MeshPtr block_ptr = MeshFactory::cube();
	auto& rod = static_cast<ColorMesh&>(*rod_ptr);
	auto& block = static_cast<ColorMesh&>(*block_ptr);
	auto rod_vertices = rod.get_vertices();
	auto block_vertices = block.get_vertices();
	auto rod_indices = rod.get_indices();
	auto block_indices = block.get_indices();

	auto rod_transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.5f));
	rod_transform = glm::scale(rod_transform, glm::vec3(INITIAL_RADIUS, INITIAL_RADIUS, 1.0f));
	rod_transform = glm::rotate(rod_transform, glm::radians(90.0f), Maths::right_vec);
	transform_vertices(rod_vertices, rod_transform);

	// to prevent z-fighting
	const float small_offset = 0.0001f;
	auto block_transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 1.0f - BLOCK_LENGTH * 0.5f + small_offset));
	block_transform = glm::scale(block_transform, glm::vec3(BLOCK_LENGTH));
	transform_vertices(block_vertices, block_transform);

	concatenate_vertices(rod_vertices, rod_indices, block_vertices, block_indices);

	auto mesh = ecs.get_mesh_system().add(std::make_unique<ColorMesh>(std::move(rod_vertices), std::move(rod_indices)));

	return Renderable::make_default(ecs, std::move(mesh));
}

void ScaleGizmoObj::point(ECS& ecs, const glm::vec3& start, const glm::vec3& end)
{
	const glm::vec3 v1 = Maths::forward_vec;
	const glm::vec3 v2 = glm::normalize(end - start);
	const glm::quat rot = Maths::RotationBetweenVectors(v1, v2);
	auto& transform = ecs.get_transformation(get_id());
	transform.set_rotation(rot);
	transform.set_position(start);
}
