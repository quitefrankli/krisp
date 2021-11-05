#pragma once

#include "graphics_engine_base_module.hpp"

#include <vulkan/vulkan.hpp>


class GraphicsEnginePipeline : public GraphicsEngineBaseModule
{
public:
	enum class PIPELINE_TYPE
	{
		STANDARD,
		WIREFRAME,
		COLOR, // no texture
	};

	GraphicsEnginePipeline(GraphicsEngine& engine, PIPELINE_TYPE pipeline_type=PIPELINE_TYPE::STANDARD);
	~GraphicsEnginePipeline();

	VkPipeline graphics_pipeline;
	VkPipelineLayout pipeline_layout;
	VkRenderPass render_pass;

private:
	void create_render_pass();
};