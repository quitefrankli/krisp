#pragma once

#include "pipeline.hpp"
#include "graphics_engine/graphics_engine_base_module.hpp"
#include "render_types.hpp"

#include <unordered_map>
#include <memory>


template<typename GraphicsEngineT>
class GraphicsEnginePipelineManager : public GraphicsEngineBaseModule<GraphicsEngineT>
{
public:
	using PipelineType = GraphicsEnginePipeline<GraphicsEngineT>;

	GraphicsEnginePipelineManager(GraphicsEngineT& engine);
	~GraphicsEnginePipelineManager();

	PipelineType& get_pipeline(ERenderType type);

private:
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_graphics_engine;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_logical_device;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_physical_device;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_instance;
	using GraphicsEngineBaseModule<GraphicsEngineT>::create_buffer;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_num_swapchain_frames;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_render_pass;
	using GraphicsEngineBaseModule<GraphicsEngineT>::should_destroy;
	std::unordered_map<ERenderType, std::unique_ptr<PipelineType>> pipelines;
};