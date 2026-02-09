#include "renderable.hpp"
#include "renderable/material_factory.hpp"
#include "renderable/mesh_factory.hpp"


Renderable Renderable::make_default()
{
	return make_default(MeshFactory::sphere_id());
}

Renderable Renderable::make_default(MeshID mesh_id)
{
	static MaterialID default_mat = MaterialFactory::fetch_preset(EMaterialPreset::DEFAULT);
	Renderable renderable;
	renderable.mesh_id = mesh_id;
	renderable.material_ids = { default_mat };
	renderable.pipeline_render_type = ERenderType::COLOR;

	return renderable;
}