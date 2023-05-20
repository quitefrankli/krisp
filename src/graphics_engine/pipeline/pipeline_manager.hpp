#pragma once

#include "pipeline.hpp"
#include "graphics_engine/graphics_engine_base_module.hpp"
#include "pipeline_types.hpp"
#include "pipeline_id.hpp"

#include <unordered_map>
#include <memory>


template<typename GraphicsEngineT>
class GraphicsEnginePipelineManager : public GraphicsEngineBaseModule<GraphicsEngineT>
{
public:
	using PipelineType = GraphicsEnginePipeline<GraphicsEngineT>;

	GraphicsEnginePipelineManager(GraphicsEngineT& engine);
	~GraphicsEnginePipelineManager();

	PipelineType* fetch_pipeline(PipelineID id);

	VkPipelineLayout get_generic_pipeline_layout() const { return generic_pipeline_layout; }

private:
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_graphics_engine;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_logical_device;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_physical_device;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_instance;
	using GraphicsEngineBaseModule<GraphicsEngineT>::create_buffer;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_num_swapchain_frames;
	using GraphicsEngineBaseModule<GraphicsEngineT>::should_destroy;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_rsrc_mgr;

	std::unique_ptr<PipelineType> create_pipeline(PipelineID id);
	template<typename PrimaryPipelineType>
	std::unique_ptr<PipelineType> create_pipeline(PipelineID id);

	std::unordered_map<EPipelineType, std::unique_ptr<PipelineType>> pipelines;
	std::unordered_map<PipelineID, std::unique_ptr<PipelineType>> pipelines_by_id;

	VkPipelineLayout generic_pipeline_layout = nullptr;
};