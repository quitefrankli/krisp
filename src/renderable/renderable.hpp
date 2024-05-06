#pragma once

#include "identifications.hpp"
// TODO: come up with a to decouiple this, having graphics engine code here is bad
#include "graphics_engine/pipeline/pipeline_types.hpp"
#include "renderable/material_group.hpp"

#include <vector>


// A renderable should encapsulate the minimum amount of information in order to be fully renderable
// There are multiple materials because we may need multiple maps i.e. texture, normal, uv maps
struct Renderable
{
	MeshID mesh_id;
	MatVec material_ids;
	EPipelineType pipeline_render_type = EPipelineType::COLOR; // TODO: this default value is not good, it should be unassigned

	static Renderable make_default(MeshID mesh_id);
};