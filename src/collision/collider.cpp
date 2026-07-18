#include "collider.hpp"
#include "game_engine.hpp"
#include "renderable/mesh_factory.hpp"
#include "renderable/material_factory.hpp"
#include "entity_component_system/mesh_system.hpp"

#include <glm/gtx/quaternion.hpp>

#include <limits>

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

bool ray_triangle_intersection(const Maths::Ray& ray,
	const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2,
	float& out_t)
{
	const glm::vec3 edge1 = p1 - p0;
	const glm::vec3 edge2 = p2 - p0;
	const glm::vec3 p = glm::cross(ray.direction, edge2);
	const float determinant = glm::dot(edge1, p);
	if (Maths::absf(determinant) <= Maths::ACCEPTABLE_FLOATING_PT_DIFF)
		return false;

	const float inverse_determinant = 1.0f / determinant;
	const glm::vec3 origin_to_triangle = ray.origin - p0;
	const float u = glm::dot(origin_to_triangle, p) * inverse_determinant;
	if (u < 0.0f || u > 1.0f)
		return false;
	const glm::vec3 q = glm::cross(origin_to_triangle, edge1);
	const float v = glm::dot(ray.direction, q) * inverse_determinant;
	if (v < 0.0f || u + v > 1.0f)
		return false;

	out_t = glm::dot(edge2, q) * inverse_determinant;
	return out_t >= 0.0f;
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
	ray.length = data.length;

	return ray;
}

Maths::Quad QuadCollider::get_data() const
{
	const auto& transform = get_temporary_transform();
	const glm::vec3 transformed_offset = transform.get_mat4() * glm::vec4(data.offset, 1.0f);
	const glm::vec3 transformed_normal = glm::normalize(transform.get_orient() * data.normal);

	// Scale the size along the quad's tangent and bitangent axes
	const glm::vec3 normal = glm::normalize(data.normal);
	const glm::vec3 ref_axis = glm::abs(normal.y) > 0.99f ? Maths::right_vec : Maths::up_vec;
	const glm::vec3 tangent = glm::normalize(glm::cross(ref_axis, normal));
	const glm::vec3 bitangent = glm::normalize(glm::cross(normal, tangent));
	const glm::mat3 scale_mat = glm::mat3(transform.get_mat4());
	const glm::vec2 transformed_size(
		data.size.x * glm::length(scale_mat * tangent),
		data.size.y * glm::length(scale_mat * bitangent));

	return Maths::Quad(transformed_offset, transformed_normal, transformed_size);
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

bool BoxCollider::check_collision(const RayCollider& ray, glm::vec3& out_intersection) const
{
	const auto ray_data = ray.get_data();
	const glm::mat4& transform = get_temporary_transform().get_mat4();
	if (Maths::absf(glm::determinant(transform)) <= std::numeric_limits<float>::epsilon())
	{
		return false;
	}

	const glm::mat4 inverse_transform = glm::inverse(transform);
	const Maths::Ray local_ray(
		glm::vec3(inverse_transform * glm::vec4(ray_data.origin, 1.0f)),
		glm::vec3(inverse_transform * glm::vec4(ray_data.direction, 0.0f)));
	glm::vec3 local_intersection;
	if (!data.check_collision(local_ray, local_intersection))
	{
		return false;
	}

	out_intersection = glm::vec3(transform * glm::vec4(local_intersection, 1.0f));
	return true;
}

Object& BoxCollider::spawn_debug_object(GameEngine& engine) const
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

void BoxCollider::update_debug_object(Object& obj) const
{
	const glm::vec3 centre = (data.min_bound + data.max_bound) * 0.5f;
	const glm::vec3 size = data.max_bound - data.min_bound;
	obj.set_transform(get_temporary_transform().get_mat4() *
		glm::translate(Maths::identity_mat, centre) * glm::scale(Maths::identity_mat, size));
}

bool MeshCollider::check_collision(const RayCollider& ray, glm::vec3& out_intersection) const
{
	const auto ray_data = ray.get_data();
	const glm::mat4& transform = get_temporary_transform().get_mat4();
	if (Maths::absf(glm::determinant(transform)) <= std::numeric_limits<float>::epsilon())
		return false;

	const glm::mat4 inverse_transform = glm::inverse(transform);
	const Maths::Ray local_ray(
		glm::vec3(inverse_transform * glm::vec4(ray_data.origin, 1.0f)),
		glm::vec3(inverse_transform * glm::vec4(ray_data.direction, 0.0f)));

	bool collided = false;
	float closest_t = std::numeric_limits<float>::infinity();
	for (const MeshID mesh_id : mesh_ids)
	{
		const MeshPickData& data = MeshSystem::get(mesh_id).get_pick_data();
		if (!data.has_triangles())
			continue;
		glm::vec3 unused_intersection;
		if (!data.get_bounds().check_collision(local_ray, unused_intersection))
			continue;

		std::vector<uint32_t> stack{ 0 };
		const auto& nodes = data.get_nodes();
		const auto& positions = data.get_positions();
		const auto& triangles = data.get_triangles();
		const auto& triangle_indices = data.get_triangle_indices();
		while (!stack.empty())
		{
			const MeshBvhNode& node = nodes[stack.back()];
			stack.pop_back();
			if (!node.bounds.check_collision(local_ray, unused_intersection))
				continue;
			if (!node.is_leaf())
			{
				stack.push_back(node.first);
				stack.push_back(node.second);
				continue;
			}

			for (uint32_t i = 0; i < node.count; ++i)
			{
				const auto& triangle = triangles[triangle_indices[node.first + i]];
				float t;
				if (ray_triangle_intersection(local_ray,
					positions[triangle.vertices[0]], positions[triangle.vertices[1]], positions[triangle.vertices[2]], t) &&
					t < closest_t)
				{
					closest_t = t;
					collided = true;
				}
			}
		}
	}

	if (!collided)
		return false;
	out_intersection = glm::vec3(transform * glm::vec4(local_ray.origin + closest_t * local_ray.direction, 1.0f));
	return true;
}

Object& MeshCollider::spawn_debug_object(GameEngine& engine) const
{
	Object& obj = engine.spawn_object<Object>(Renderable{
		.mesh_id = MeshFactory::cube_id(),
		.material_ids = { MaterialFactory::fetch_preset(EMaterialPreset::GIZMO_ARC) },
		.pipeline_render_type = ERenderType::COLOR,
		.casts_shadow = false,
	});
	obj.set_name("Collider Visual");
	update_debug_object(obj);
	return obj;
}

void MeshCollider::update_debug_object(Object& obj) const
{
	bool has_bounds = false;
	AABB bounds;
	for (const MeshID mesh_id : mesh_ids)
	{
		const auto& data = MeshSystem::get(mesh_id).get_pick_data();
		if (!data.has_bounds())
			continue;
		if (!has_bounds)
		{
			bounds = data.get_bounds();
			has_bounds = true;
		}
		else
			bounds.min_max(data.get_bounds());
	}
	if (!has_bounds)
		return;
	const glm::vec3 centre = (bounds.min_bound + bounds.max_bound) * 0.5f;
	obj.set_transform(get_temporary_transform().get_mat4() * glm::translate(Maths::identity_mat, centre) *
		glm::scale(Maths::identity_mat, bounds.max_bound - bounds.min_bound));
}
