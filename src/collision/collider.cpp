#include "collider.hpp"


Maths::Ray RayCollider::get_data() const
{
	Maths::Ray ray;
	const auto& transform = get_temporary_transform();

	ray.origin = transform.get_mat4() * glm::vec4(data.origin, 1.0f);
	ray.direction = glm::normalize(transform.get_orient() * data.direction);

	return ray;
}

Maths::Plane QuadCollider::get_data() const
{
	Maths::Plane plane;
	const auto& transform = get_temporary_transform();

	plane.offset = transform.get_mat4() * glm::vec4(data.offset, 1.0f);
	plane.normal = glm::normalize(transform.get_orient() * data.normal);

	return plane;
}

bool QuadCollider::check_collision(const RayCollider& ray) const
{
	return Maths::check_ray_plane_intersection(ray.get_data(), get_data());
}

bool QuadCollider::check_collision(const RayCollider& ray, glm::vec3& out_intersection) const
{
	const auto plane = get_data();
	const auto ray_data = ray.get_data();
	if (!Maths::check_ray_plane_intersection(ray_data, plane))
	{
		return false;
	}

	out_intersection = Maths::ray_plane_intersection(ray_data, plane);
	return is_point_in_quad_bounds(out_intersection, plane);
}

bool QuadCollider::is_point_in_quad_bounds(const glm::vec3& point, const Maths::Plane& plane) const
{
	const auto& transform = get_temporary_transform();
	const glm::vec3 normal = glm::normalize(plane.normal);
	const glm::vec3 ref_axis = glm::abs(normal.y) > 0.99f ? Maths::right_vec : Maths::up_vec;
	const glm::vec3 tangent = glm::normalize(glm::cross(ref_axis, normal));
	const glm::vec3 bitangent = glm::normalize(glm::cross(normal, tangent));

	const glm::vec3 delta = point - plane.offset;
	const float local_x = glm::dot(delta, tangent);
	const float local_y = glm::dot(delta, bitangent);
	const float half_width = 0.5f * size.x * Maths::absf(transform.get_scale().x);
	const float half_height = 0.5f * size.y * Maths::absf(transform.get_scale().y);

	return Maths::absf(local_x) <= half_width && Maths::absf(local_y) <= half_height;
}

Maths::Sphere SphereCollider::get_data() const
{
	Maths::Sphere sphere;
	const auto& transform = get_temporary_transform();

	sphere.origin = transform.get_mat4() * glm::vec4(data.origin, 1.0f);
	sphere.radius = (transform.get_scale().x + transform.get_scale().y + transform.get_scale().z)/3.0f * data.radius;

	return sphere;
}
