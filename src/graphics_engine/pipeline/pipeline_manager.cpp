#include "pipeline_manager.hpp"


GraphicsEnginePipelineManager::GraphicsEnginePipelineManager(GraphicsEngine& engine) :
	GraphicsEngineBaseModule(engine),
	pipeline_main(engine, EPipelineType::STANDARD),
	pipeline_color(engine, EPipelineType::COLOR),
	pipeline_wireframe(engine, EPipelineType::WIREFRAME),
	pipeline_light_source(engine, EPipelineType::LIGHT_SOURCE)
{
}

GraphicsEnginePipeline& GraphicsEnginePipelineManager::get_pipeline(EPipelineType type)
{
	switch (type)
	{
		case EPipelineType::STANDARD:
			return pipeline_main;
		case EPipelineType::COLOR:
			return pipeline_color;
		case EPipelineType::WIREFRAME:
			return pipeline_wireframe;
		case EPipelineType::LIGHT_SOURCE:
			return pipeline_light_source;
		default:
			throw std::runtime_error("GraphicsEnginePipelineManager::get_pipeline: invalid pipeline type");
	};
}