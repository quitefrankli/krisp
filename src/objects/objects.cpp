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
	renderable.mesh_id = MeshFactory::arrow_id(RADIUS, nVertices);
	renderable.material_ids = { MaterialFactory::fetch_preset(EMaterialPreset::GIZMO_ARROW) };
	renderables = { std::move(renderable) };
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
	const int nVertices = 8;
	
	Renderable renderable;
	renderable.mesh_id = MeshFactory::arc_id(nVertices, outer_radius, inner_radius);
	renderable.material_ids.push_back(MaterialFactory::fetch_preset(EMaterialPreset::GIZMO_ARC));
	renderables.push_back(std::move(renderable));
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