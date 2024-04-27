#include "renderable.hpp"

#include "entity_component_system/material_system.hpp"


Renderable Renderable::make_default(MeshID mesh_id)
{
	static MaterialID default_mat = MaterialSystem::add(std::make_unique<Material>());
	Renderable renderable;
	renderable.mesh_id = mesh_id;
	renderable.material_ids = { default_mat };
	renderable.pipeline_render_type = EPipelineType::COLOR;

	return renderable;
}