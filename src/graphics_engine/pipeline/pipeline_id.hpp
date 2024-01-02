#pragma once

#include "pipeline_types.hpp"

#include <set>
#include <functional>


enum class EPipelineModifier
{
	NONE,
	STENCIL,
	POST_STENCIL,
	WIREFRAME,
	SHADOW_MAP
};

struct PipelineID
{
	EPipelineType primary_pipeline_type;
	EPipelineModifier pipeline_modifier = EPipelineModifier::NONE;

	bool operator==(const PipelineID& other) const
	{
		return primary_pipeline_type == other.primary_pipeline_type && 
			pipeline_modifier == other.pipeline_modifier;
	}
};

template<>
struct std::hash<PipelineID>
{
	size_t operator()(const PipelineID& pipeline_id) const
	{
		return std::hash<int>()(static_cast<int>(pipeline_id.primary_pipeline_type)) ^ 
			std::hash<int>()(static_cast<int>(pipeline_id.pipeline_modifier));
	}
};