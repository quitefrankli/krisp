#pragma once

#include "graphics_engine/graphics_engine_base_module.hpp"
#include "render_types.hpp"

#include <vulkan/vulkan.hpp>


class GraphicsEnginePipeline : public GraphicsEngineBaseModule
{
public:
	GraphicsEnginePipeline(GraphicsEngine& engine, ERenderType render_type);
	GraphicsEnginePipeline(GraphicsEnginePipeline&&) noexcept;
	~GraphicsEnginePipeline();

	VkPipeline graphics_pipeline;
	VkPipelineLayout pipeline_layout;

private:
	const ERenderType render_type;
};