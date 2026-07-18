#include "collider_ecs.hpp"
#include "ecs.hpp"
#include "entity_component_system/mesh_system.hpp"
#include "utility.hpp"
#include "collision/collision_detector.hpp"
#include "serialization/serialization_helpers.hpp"

#include <quill/LogMacros.h>

#include <limits>
#include <algorithm>
#include <vector>

namespace
{
std::string collider_path(const std::size_t index, const std::string_view field)
{
	return "$.collider_system[" + std::to_string(index) + "]." + std::string(field);
}
}


void ColliderSystem::add_collider(EntityID id, std::unique_ptr<Collider>&& collider) 
{
	add_collider(id, std::move(collider), Maths::Transform{});
}

void ColliderSystem::add_collider(EntityID id, std::unique_ptr<Collider>&& collider, 
								  const Maths::Transform& offset) 
{
	ColliderComponent new_component;
	new_component.collider = std::move(collider);
	new_component.collider->apply_transform(offset);

	components.emplace(id, std::move(new_component));
}

void ColliderSystem::add_mesh_collider(const EntityID id)
{
	std::vector<MeshID> mesh_ids;
	bool has_triangles = false;
	bool has_bounds = false;
	AABB combined_bounds;
	for (const auto& renderable : get_ecs().get_object(id).renderables)
	{
		const auto& pick_data = MeshSystem::get(renderable.mesh_id).get_pick_data();
		mesh_ids.push_back(renderable.mesh_id);
		has_triangles = has_triangles || pick_data.has_triangles();
		if (!pick_data.has_bounds())
			continue;
		if (!has_bounds)
		{
			combined_bounds = pick_data.get_bounds();
			has_bounds = true;
		}
		else
			combined_bounds.min_max(pick_data.get_bounds());
	}

	if (has_triangles)
		add_collider(id, std::make_unique<MeshCollider>(std::move(mesh_ids)));
	else if (has_bounds)
		add_collider(id, std::make_unique<BoxCollider>(combined_bounds));
	else
		LOG_WARNING(Utility::get_logger(), "ColliderSystem: Entity {} has no pickable mesh geometry", id.get_underlying());
}

const Collider* ColliderSystem::get_collider(EntityID id) const
{
	auto it = components.find(id);
	if (it == components.end())
	{
		return nullptr;
	}

	Collider* collider = it->second.collider.get();
	collider->set_temporary_transform(get_ecs().get_object(id).get_maths_transform());

	return collider;
}

void ColliderSystem::serialize(Serializer& out) const
{
	std::vector<EntityID> ids;
	ids.reserve(components.size());
	for (const auto& [id, _] : components)
		ids.push_back(id);
	std::ranges::sort(ids);

	auto entries = out.sequence("collider_system");
	for (std::size_t index = 0; index < ids.size(); ++index) {
		const auto id = ids[index];
		const Collider* collider = components.at(id).collider.get();
		auto entry = entries.append_map();
		entry.write("entity_id", id.get_underlying());
		auto data = entry.map("data");
		switch (collider->get_type()) {
		case ECollider::RAY: {
			const auto* typed = dynamic_cast<const RayCollider*>(collider);
			if (!typed)
				throw SerializationError("Invalid ray collider at " + collider_path(index, "type"));
			entry.write("type", "ray");
			const auto& ray = typed->get_local_data();
			Serialization::write_vec3(data, "origin", ray.origin);
			Serialization::write_vec3(data, "direction", ray.direction);
			data.write("length", ray.length);
			break;
		}
		case ECollider::SPHERE: {
			const auto* typed = dynamic_cast<const SphereCollider*>(collider);
			if (!typed)
				throw SerializationError("Invalid sphere collider at " + collider_path(index, "type"));
			entry.write("type", "sphere");
			const auto& sphere = typed->get_local_data();
			Serialization::write_vec3(data, "origin", sphere.origin);
			data.write("radius", sphere.radius);
			break;
		}
		case ECollider::QUAD: {
			const auto* typed = dynamic_cast<const QuadCollider*>(collider);
			if (!typed)
				throw SerializationError("Invalid quad collider at " + collider_path(index, "type"));
			entry.write("type", "quad");
			const auto& quad = typed->get_local_data();
			Serialization::write_vec3(data, "offset", quad.offset);
			Serialization::write_vec3(data, "normal", quad.normal);
			Serialization::write_vec2(data, "size", quad.size);
			break;
		}
		case ECollider::BOX: {
			const auto* typed = dynamic_cast<const BoxCollider*>(collider);
			if (!typed)
				throw SerializationError("Invalid box collider at " + collider_path(index, "type"));
			entry.write("type", "box");
			const auto& bounds = typed->get_local_data();
			Serialization::write_vec3(data, "minimum", bounds.min_bound);
			Serialization::write_vec3(data, "maximum", bounds.max_bound);
			break;
		}
		case ECollider::MESH: {
			const auto* typed = dynamic_cast<const MeshCollider*>(collider);
			if (!typed)
				throw SerializationError("Invalid mesh collider at " + collider_path(index, "type"));
			entry.write("type", "mesh");
			auto mesh_ids = data.sequence("mesh_ids");
			for (const auto mesh_id : typed->get_mesh_ids())
				mesh_ids.append(mesh_id.get_underlying());
			break;
		}
		default:
			throw SerializationError("Unsupported collider type at " + collider_path(index, "type"));
		}
	}
}

void ColliderSystem::deserialize(const Deserializer& in)
{
	std::unordered_map<EntityID, ColliderComponent> restored;
	const auto entries = in.child("collider_system").elements();
	for (std::size_t index = 0; index < entries.size(); ++index) {
		const auto& entry = entries[index];
		const EntityID id(entry.read<std::uint64_t>("entity_id"));
		const auto type = entry.read<std::string>("type");
		const auto data = entry.child("data");
		std::unique_ptr<Collider> collider;
		if (type == "ray") {
			Maths::Ray ray(
				Serialization::read_vec3(data, "origin"),
				Serialization::read_vec3(data, "direction"));
			ray.length = data.read<float>("length");
			collider = std::make_unique<RayCollider>(ray);
		} else if (type == "sphere") {
			collider = std::make_unique<SphereCollider>(Maths::Sphere(
				Serialization::read_vec3(data, "origin"), data.read<float>("radius")));
		} else if (type == "quad") {
			collider = std::make_unique<QuadCollider>(
				Maths::Plane(Serialization::read_vec3(data, "offset"),
					Serialization::read_vec3(data, "normal")),
				Serialization::read_vec2(data, "size"));
		} else if (type == "box") {
			collider = std::make_unique<BoxCollider>(AABB(
				Serialization::read_vec3(data, "minimum"),
				Serialization::read_vec3(data, "maximum")));
		} else if (type == "mesh") {
			std::vector<MeshID> mesh_ids;
			for (const auto& mesh_id : data.child("mesh_ids").elements())
				mesh_ids.emplace_back(mesh_id.as<std::uint64_t>());
			collider = std::make_unique<MeshCollider>(std::move(mesh_ids));
		} else {
			throw SerializationError("Unsupported collider type at " + collider_path(index, "type"));
		}
		ColliderComponent component{ .collider = std::move(collider) };
		if (!restored.emplace(id, std::move(component)).second)
			throw SerializationError("Duplicate collider entity at " + collider_path(index, "entity_id"));
	}
	components = std::move(restored);
}

DetectedEntityCollision ColliderSystem::raycast(const Maths::Ray& ray, const std::optional<EntityID> ignored) const
{
	DetectedEntityCollision result;
	float closest_distance = std::numeric_limits<float>::infinity();
	RayCollider ray_collider(ray);
	for (const auto& [id, _] : components)
	{
		if (ignored && id == *ignored)
			continue;
		const Collider* collider = get_collider(id);
		const CollisionResult hit = CollisionDetector::check_collision(&ray_collider, collider);
		if (!hit.bCollided)
			continue;
		const float distance = glm::distance2(ray.origin, hit.intersection);
		if (distance < closest_distance)
		{
			closest_distance = distance;
			result = { true, id, hit.intersection };
		}
	}
	return result;
}
