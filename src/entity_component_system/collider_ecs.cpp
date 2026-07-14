#include "collider_ecs.hpp"
#include "ecs.hpp"
#include "entity_component_system/mesh_system.hpp"
#include "utility.hpp"

#include <quill/LogMacros.h>


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
