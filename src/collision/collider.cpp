#include "collider.hpp"


Maths::Ray RayCollider::get_data() const
{
	Maths::Ray ray;
	const auto& transform = get_temporary_transform();

	ray.origin = transform.get_mat4() * glm::vec4(data.origin, 1.0f);
	ray.direction = glm::normalize(transform.get_orient() * data.direction);

	return ray;
}

Maths::Sphere SphereCollider::get_data() const
{
	Maths::Sphere sphere;
	const auto& transform = get_temporary_transform();

	sphere.origin = transform.get_mat4() * glm::vec4(data.origin, 1.0f);
	sphere.radius = (transform.get_scale().x + transform.get_scale().y + transform.get_scale().z)/3.0f * data.radius;

	return sphere;
}
