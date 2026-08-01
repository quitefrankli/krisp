#include "renderable.hpp"
#include "entity_component_system/ecs.hpp"
#include "renderable/material_factory.hpp"
#include "renderable/mesh_factory.hpp"


Renderable Renderable::make_default(ECS& ecs)
{
	return make_default(ecs, ecs.get_mesh_system().add(MeshFactory::sphere()));
}

MeshID Renderable::get_mesh_id() const
{
	return mesh_owner->get_id();
}

MaterialID Renderable::get_material_id(const size_t index) const
{
	return material_owners.at(index)->get_id();
}

MatVec Renderable::get_material_ids() const
{
	MatVec result;
	result.reserve(material_owners.size());
	for (const auto& material_owner : material_owners)
		result.push_back(material_owner->get_id());
	return result;
}

Renderable Renderable::make_default(ECS& ecs, MeshHandle mesh_owner)
{
	Renderable renderable;
	renderable.mesh_owner = std::move(mesh_owner);
	renderable.material_owners.push_back(
		ecs.get_material_system().add(MaterialFactory::fetch_preset(EMaterialPreset::DEFAULT)));
	renderable.pipeline_render_type = ERenderType::COLOR;
	return renderable;
}
