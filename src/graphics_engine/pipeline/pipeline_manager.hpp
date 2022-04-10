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
	VkPipelineLayout& get_main_pipeline_layout();
	VkRenderPass get_main_pipeline_render_pass();

private:
	std::unordered_map<ERenderType, GraphicsEnginePipeline> pipelines;

	void create_render_pass();

	// for now the render_pass is simply shared between pipelines
	VkRenderPass render_pass;
};