#include "objects.hpp"
#include "renderable/mesh_factory.hpp"
#include "renderable/material_factory.hpp"
#include "renderable/material.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <iostream>


Arrow::Arrow()
{
	const int nVertices = 8;
	Renderable renderable;
	renderable.mesh_id = MeshFactory::arrow_id(INITIAL_RADIUS, nVertices);
	renderable.material_ids = { MaterialFactory::fetch_preset(EMaterialPreset::GIZMO_ARROW) };
	renderables = { std::move(renderable) };
}

Arrow::Arrow(const glm::vec3& start, const glm::vec3& end) : Arrow()
{
	point(start, end);
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

ArcObject::ArcObject()
{
	const int nVertices = 8;
	
	Renderable renderable;
	renderable.mesh_id = MeshFactory::arc_id(nVertices, INITIAL_OUTER_RAIUS, INITIAL_INNER_RADIUS);
	renderable.material_ids.push_back(MaterialFactory::fetch_preset(EMaterialPreset::GIZMO_ARC));
	renderables.push_back(std::move(renderable));
}
