#pragma once

#include "renderable/render_types.hpp"

#include <set>
#include <functional>


enum class EPipelineModifier
{
	NONE,
	STENCIL,
	POST_STENCIL,
	WIREFRAME,
	UNLIT_BASE_COLOR,
	SHADOW_MAP
};

struct PipelineID
{
	ERenderType primary_pipeline_type;
	EPipelineModifier pipeline_modifier = EPipelineModifier::NONE;
	EAlphaMode alpha_mode = EAlphaMode::OPAQUE;

	bool operator==(const PipelineID& other) const
	{
		return primary_pipeline_type == other.primary_pipeline_type && 
		pipeline_modifier == other.pipeline_modifier && alpha_mode == other.alpha_mode;
	}
};

template<>
struct std::hash<PipelineID>
{
	size_t operator()(const PipelineID& pipeline_id) const
	{
		return std::hash<int>()(static_cast<int>(pipeline_id.primary_pipeline_type)) ^ 
			(std::hash<int>()(static_cast<int>(pipeline_id.pipeline_modifier)) << 1) ^
			(std::hash<int>()(static_cast<int>(pipeline_id.alpha_mode)) << 2);
	}
};
