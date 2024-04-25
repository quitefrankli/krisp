#pragma once

#include "identifications.hpp"
// TODO: come up with a to decouiple this, having graphics engine code here is bad
#include "graphics_engine/pipeline/pipeline_types.hpp"

#include <vector>


// A renderable should encapsulate the minimum amount of information in order to be fully renderable
// There are multiple materials because we may need multiple maps i.e. texture, normal, uv maps
struct Renderable
{
	MeshID mesh;
	std::vector<MaterialID> materials;
	EPipelineType pipeline_render_type = EPipelineType::COLOR; // TODO: this default value is not good, it should be unassigned
};