#pragma once

#include "graphics_engine/graphics_engine_base_module.hpp"
#include "pipeline.hpp"


class GraphicsEnginePipelineManager : public GraphicsEngineBaseModule
{
public:
	GraphicsEnginePipelineManager(GraphicsEngine& engine);

	GraphicsEnginePipeline& get_pipeline(EPipelineType type);
	VkPipelineLayout& get_main_pipeline_layout() { return pipeline_main.pipeline_layout; }
	VkRenderPass& get_main_pipeline_render_pass() { return pipeline_main.render_pass; }

private:
	GraphicsEnginePipeline pipeline_main;
	GraphicsEnginePipeline pipeline_wireframe;
	GraphicsEnginePipeline pipeline_color;
	GraphicsEnginePipeline pipeline_light_source;
};