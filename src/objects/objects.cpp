#include "objects.hpp"
#include "renderable/mesh_factory.hpp"
#include "renderable/material_factory.hpp"
#include "renderable/material.hpp"
#include "entity_component_system/ecs.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <iostream>


Renderable Arrow::make_renderable(ECS& ecs)
{
	const int nVertices = 8;
	Renderable renderable;
	renderable.shading_mode = EShadingMode::UNLIT;
	renderable.casts_shadow = false;
	renderable.mesh_owner = ecs.get_mesh_system().add(MeshFactory::arrow(INITIAL_RADIUS, nVertices));
	renderable.material_owners.push_back(
		ecs.get_material_system().add(MaterialFactory::fetch_preset(EMaterialPreset::GIZMO_ARROW)));
	return renderable;
}

void Arrow::point(ECS& ecs, const glm::vec3& start, const glm::vec3& end)
{
	const auto& v1 = Maths::forward_vec;
	const auto v2 = glm::normalize(end - start);
	const glm::quat rot = Maths::RotationBetweenVectors(v1, v2);
	auto& transform = ecs.get_transformation(get_id());
	transform.set_rotation(rot);
	transform.set_position(start);

	auto scale = transform.get_scale();
	scale.z = glm::distance(start, end);
	transform.set_scale(scale);
}

Renderable ArcObject::make_renderable(ECS& ecs)
{
	const int nVertices = 8;

	Renderable renderable;
	renderable.shading_mode = EShadingMode::UNLIT;
	renderable.casts_shadow = false;
	renderable.mesh_owner = ecs.get_mesh_system().add(
		MeshFactory::arc(nVertices, INITIAL_OUTER_RAIUS, INITIAL_INNER_RADIUS));
	renderable.material_owners.push_back(
		ecs.get_material_system().add(MaterialFactory::fetch_preset(EMaterialPreset::GIZMO_ARC)));
	return renderable;
}
