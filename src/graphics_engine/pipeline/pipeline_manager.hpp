#pragma once

#include "pipeline.hpp"
#include "graphics_engine/graphics_engine_base_module.hpp"
#include "renderable/render_types.hpp"
#include "pipeline_id.hpp"

#include <unordered_map>
#include <memory>


class GraphicsEnginePipelineManager : public GraphicsEngineBaseModule
{
public:
	using PipelineType = GraphicsEnginePipeline;

	GraphicsEnginePipelineManager(GraphicsEngine& engine);
	~GraphicsEnginePipelineManager();

	PipelineType* fetch_pipeline(PipelineID id);

	VkPipelineLayout get_generic_pipeline_layout() const { return generic_pipeline_layout; }

private:

	std::unique_ptr<PipelineType> create_pipeline(PipelineID id);
	template<typename PrimaryPipelineType>
	std::unique_ptr<PipelineType> create_pipeline(PipelineID id);

	std::unordered_map<ERenderType, std::unique_ptr<PipelineType>> pipelines;
	std::unordered_map<PipelineID, std::unique_ptr<PipelineType>> pipelines_by_id;

	VkPipelineLayout generic_pipeline_layout = nullptr;
};