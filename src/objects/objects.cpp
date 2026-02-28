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

bool Arrow::check_collision(const Maths::Ray& ray)
{
	assert(glm::epsilonEqual(get_scale().x, get_scale().y, 0.001f));
	const glm::vec3 axis = get_rotation() * Maths::forward_vec;
	const float length = get_scale().z;
	const float radius = INITIAL_RADIUS * get_scale().x;
	return Maths::check_ray_rod_collision(ray, 
										  get_position(), 
										  get_position() + axis * length, 
										  radius);
}

bool Arrow::check_collision(const Maths::Ray& ray, glm::vec3& intersection) const
{
	assert(glm::epsilonEqual(get_scale().x, get_scale().y, 0.001f));
	const glm::vec3 axis = get_rotation() * Maths::forward_vec;
	const float length = get_scale().z;
	const float radius = INITIAL_RADIUS * get_scale().x;
	return Maths::check_ray_rod_collision(ray, 
										  get_position(), 
										  get_position() + axis * length, 
										  radius,
										  intersection);
}

ArcObject::ArcObject()
{
	const int nVertices = 8;
	
	Renderable renderable;
	renderable.mesh_id = MeshFactory::arc_id(nVertices, INITIAL_OUTER_RAIUS, INITIAL_INNER_RADIUS);
	renderable.material_ids.push_back(MaterialFactory::fetch_preset(EMaterialPreset::GIZMO_ARC));
	renderables.push_back(std::move(renderable));
}

bool ArcObject::check_collision(const Maths::Ray& ray)
{
	glm::vec3 intersection;
	return check_collision(ray, intersection);
}

bool ArcObject::check_collision(const Maths::Ray& ray, glm::vec3& intersection) const
{
	// imagine an arc as a plane, the default has a normal = upvector
	const Maths::Plane plane(get_position(), glm::normalize(get_rotation() * Maths::forward_vec));

	// first check if there is even an intersection
	if (!Maths::check_ray_plane_intersection(ray, plane))
		return false;

	intersection = Maths::ray_plane_intersection(ray, plane);
	const float dist = glm::distance(intersection, plane.offset);
	assert(glm::epsilonEqual(get_scale().x, get_scale().y, 0.001f));
	const float outer_radius = INITIAL_OUTER_RAIUS * get_scale().x;
	const float inner_radius = INITIAL_INNER_RADIUS * get_scale().x;
	if (dist > outer_radius || dist < inner_radius)
	{
		return false;
	}

	// this might be a tad inefficient but will do for now
	// but it essentially unrotates the mathematical representation of the arc
	// and checks if P lies within the arc
	glm::vec3 origP = glm::inverse(get_rotation()) * (intersection - plane.offset);
	return origP.x > 0 && origP.y > 0;
}