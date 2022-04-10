#pragma once

#include "pipeline.hpp"
#include "graphics_engine/graphics_engine_base_module.hpp"
#include "render_types.hpp"

#include <unordered_map>


class GraphicsEnginePipelineManager : public GraphicsEngineBaseModule
{
public:
	GraphicsEnginePipelineManager(GraphicsEngine& engine);
	~GraphicsEnginePipelineManager();

	GraphicsEnginePipeline& get_pipeline(ERenderType type);

private:
	std::unordered_map<ERenderType, GraphicsEnginePipeline> pipelines;
};