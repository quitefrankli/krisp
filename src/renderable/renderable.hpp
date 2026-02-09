#pragma once

#include "identifications.hpp"
#include "renderable/render_types.hpp"
#include "renderable/material_group.hpp"

#include <vector>
#include <optional>


// A renderable should encapsulate the minimum amount of information in order to be fully renderable
// There are multiple materials because we may need multiple maps i.e. texture, normal, uv maps
struct Renderable
{
	MeshID mesh_id;
	MatVec material_ids;
	std::optional<SkeletonID> skeleton_id = std::nullopt;
	ERenderType pipeline_render_type = ERenderType::COLOR; // TODO: this default value is not good, it should be unassigned
	bool casts_shadow = true;

	static Renderable make_default();
	static Renderable make_default(MeshID mesh_id);
};