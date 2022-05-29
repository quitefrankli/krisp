#pragma once

#include "graphics_engine/graphics_engine_base_module.hpp"
#include "render_types.hpp"

#include <vulkan/vulkan.hpp>


template<typename GraphicsEngineT>
class GraphicsEnginePipeline : public GraphicsEngineBaseModule<GraphicsEngineT>
{
public:
	GraphicsEnginePipeline(GraphicsEngineT& engine, ERenderType render_type);
	GraphicsEnginePipeline(GraphicsEnginePipeline&&) noexcept;
	~GraphicsEnginePipeline();

	VkPipeline graphics_pipeline;
	VkPipelineLayout pipeline_layout;

private:
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_graphics_engine;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_logical_device;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_physical_device;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_instance;
	using GraphicsEngineBaseModule<GraphicsEngineT>::create_buffer;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_num_swapchain_frames;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_render_pass;
	using GraphicsEngineBaseModule<GraphicsEngineT>::should_destroy;
	const ERenderType render_type;
};