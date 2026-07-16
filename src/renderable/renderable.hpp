#pragma once

#include "identifications.hpp"
#include "renderable/render_types.hpp"
#include "renderable/material_group.hpp"

#include <vector>


// A renderable encapsulates per-draw information; object-level state such as the skeleton lives on Object.
// There are multiple materials because we may need multiple maps i.e. texture, normal, uv maps
struct Renderable
{
	MeshID mesh_id;
	MatVec material_ids;
	TexturedMaterialProperties textured_material;
	ERenderType pipeline_render_type = ERenderType::COLOR; // TODO: this default value is not good, it should be unassigned
	bool casts_shadow = true;
	bool render_on_top = false;

	static Renderable make_default();
	static Renderable make_default(MeshID mesh_id);
};
