#include "collider.hpp"
#include "game_engine.hpp"
#include "renderable/mesh_factory.hpp"
#include "renderable/material_factory.hpp"

#include <glm/gtx/quaternion.hpp>

namespace
{
glm::quat quad_normal_to_rotation(const glm::vec3& normal)
{
	const glm::vec3 normalized_normal = glm::normalize(normal);
	const glm::vec3 ref_axis = glm::abs(normalized_normal.y) > 0.99f ? Maths::right_vec : Maths::up_vec;
	const glm::vec3 tangent = glm::normalize(glm::cross(ref_axis, normalized_normal));
	const glm::vec3 bitangent = glm::normalize(glm::cross(normalized_normal, tangent));
	return glm::quat_cast(glm::mat3(tangent, bitangent, normalized_normal));
}

}

Object& RayCollider::spawn_debug_object(GameEngine& engine) const
{
	Object& obj = engine.spawn_object<Arrow>(data.origin, data.origin + data.direction * data.length);
	obj.set_name("Collider Visual");
	update_debug_object(obj);
	return obj;
}

void RayCollider::update_debug_object(Object& obj) const
{
	const auto ray = get_data();
	obj.set_position(ray.origin);
	if (glm::length2(ray.direction) > Maths::ACCEPTABLE_FLOATING_PT_DIFF)
	{
		obj.set_rotation(Maths::Vec2Rot(glm::normalize(ray.direction)));
	}
	obj.set_scale(glm::vec3(std::max(ray.length, 0.001f)));
}


Maths::Ray RayCollider::get_data() const
{
	Maths::Ray ray;
	const auto& transform = get_temporary_transform();

	ray.origin = transform.get_mat4() * glm::vec4(data.origin, 1.0f);
	ray.direction = glm::normalize(transform.get_orient() * data.direction);

	return ray;
}

Maths::Quad QuadCollider::get_data() const
{
	return data;
}

Object& QuadCollider::spawn_debug_object(GameEngine& engine) const
{	
	static Renderable renderable{
		.mesh_id = MeshFactory::cube_id(),
		.material_ids = { MaterialFactory::fetch_preset(EMaterialPreset::GIZMO_ARC) },
		.pipeline_render_type = ERenderType::COLOR,
		.casts_shadow = false,
	};
	Object& obj = engine.spawn_object<Object>(renderable);
	obj.set_name("Collider Visual");
	update_debug_object(obj);
	return obj;
}

void QuadCollider::update_debug_object(Object& obj) const
{
	const float thickness = 0.1f;

	obj.set_position(data.offset);
	obj.set_scale(glm::vec3(data.size.x, thickness, data.size.y));
}

bool QuadCollider::check_collision(const RayCollider& ray) const
{
	return Maths::check_ray_plane_intersection(ray.get_data(), get_data());
}

bool QuadCollider::check_collision(const RayCollider& ray, glm::vec3& out_intersection) const
{
	const auto quad = get_data();
	const auto ray_data = ray.get_data();

	if (!Maths::check_ray_plane_intersection(ray_data, quad))
	{
		return false;
	}

	out_intersection = Maths::ray_plane_intersection(ray_data, quad);
	return is_point_in_quad_bounds(out_intersection, quad);
}

bool QuadCollider::is_point_in_quad_bounds(const glm::vec3& point, const Maths::Quad& quad) const
{
	const auto& transform = get_temporary_transform();
	const glm::vec3 normal = glm::normalize(quad.normal);
	const glm::vec3 ref_axis = glm::abs(normal.y) > 0.99f ? Maths::right_vec : Maths::up_vec;
	const glm::vec3 tangent = glm::normalize(glm::cross(ref_axis, normal));
	const glm::vec3 bitangent = glm::normalize(glm::cross(normal, tangent));

	const glm::vec3 delta = point - quad.offset;
	const float local_x = glm::dot(delta, tangent);
	const float local_y = glm::dot(delta, bitangent);
	const float half_width = 0.5f * quad.size.x;
	const float half_height = 0.5f * quad.size.y;

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

Object& SphereCollider::spawn_debug_object(GameEngine& engine) const
{
	Object& obj = engine.spawn_object<Object>(Renderable{
		.mesh_id = MeshFactory::sphere_id(MeshFactory::EVertexType::COLOR, MeshFactory::GenerationMethod::UV_SPHERE, 64),
		.material_ids = { MaterialFactory::fetch_preset(EMaterialPreset::GIZMO_ARC) },
		.pipeline_render_type = ERenderType::COLOR,
		.casts_shadow = false,
	});
	obj.set_name("Collider Visual");
	update_debug_object(obj);
	return obj;
}

void SphereCollider::update_debug_object(Object& obj) const
{
	const auto sphere = get_data();
	obj.set_position(sphere.origin);
	obj.set_rotation(Maths::identity_quat);
	obj.set_scale(glm::vec3(sphere.radius * 2.0f));
}
