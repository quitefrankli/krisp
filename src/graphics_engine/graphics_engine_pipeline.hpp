#pragma once

#include "graphics_engine_base_module.hpp"

#include <vulkan/vulkan.hpp>


class GraphicsEnginePipeline : public GraphicsEngineBaseModule
{
public:
	GraphicsEnginePipeline(GraphicsEngine& engine);
	~GraphicsEnginePipeline();

	VkPipeline graphics_pipeline;
	VkPipelineLayout pipeline_layout;
	VkRenderPass render_pass;

private:
	void create_render_pass();
};