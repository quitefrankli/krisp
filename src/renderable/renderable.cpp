#include "renderable.hpp"
#include "renderable/material_factory.hpp"
#include "renderable/mesh_factory.hpp"


Renderable Renderable::make_default()
{
	return make_default(MeshSystem::add(MeshFactory::sphere()));
}

MeshID Renderable::get_mesh_id() const
{
	return MeshSystem::get_id(mesh_owner);
}

MaterialID Renderable::get_material_id(const size_t index) const
{
	return MaterialSystem::get_id(material_owners.at(index));
}

MatVec Renderable::get_material_ids() const
{
	MatVec result;
	result.reserve(material_owners.size());
	for (const auto& material_owner : material_owners)
		result.push_back(MaterialSystem::get_id(material_owner));
	return result;
}

Renderable Renderable::make_default(MeshHandle mesh_owner)
{
	Renderable renderable;
	renderable.mesh_owner = std::move(mesh_owner);
	renderable.material_owners.push_back(
		MaterialSystem::add(MaterialFactory::fetch_preset(EMaterialPreset::DEFAULT)));
	renderable.pipeline_render_type = ERenderType::COLOR;
	return renderable;
}
