#include "pipeline_manager.hpp"


GraphicsEnginePipelineManager::GraphicsEnginePipelineManager(GraphicsEngine& engine) :
	GraphicsEngineBaseModule(engine),
	pipeline_main(engine, EPipelineType::STANDARD),
	pipeline_color(engine, EPipelineType::COLOR),
	pipeline_wireframe(engine, EPipelineType::WIREFRAME)
{
}

GraphicsEnginePipeline& GraphicsEnginePipelineManager::get_pipeline(EPipelineType type)
{
	switch (type)
	{
		case EPipelineType::COLOR:
			return pipeline_color;
		case EPipelineType::WIREFRAME:
			return pipeline_wireframe;
		case EPipelineType::STANDARD:
		default:
			return pipeline_main;
	};
}