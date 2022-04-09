#include "pipeline_manager.hpp"


GraphicsEnginePipelineManager::GraphicsEnginePipelineManager(GraphicsEngine& engine) :
	GraphicsEngineBaseModule(engine),
	pipeline_main(engine, ERenderType::STANDARD),
	pipeline_color(engine, ERenderType::COLOR),
	pipeline_wireframe(engine, ERenderType::WIREFRAME),
	pipeline_light_source(engine, ERenderType::LIGHT_SOURCE)
{
}

GraphicsEnginePipeline& GraphicsEnginePipelineManager::get_pipeline(ERenderType type)
{
	switch (type)
	{
		case ERenderType::STANDARD:
			return pipeline_main;
		case ERenderType::COLOR:
			return pipeline_color;
		case ERenderType::WIREFRAME:
			return pipeline_wireframe;
		case ERenderType::LIGHT_SOURCE:
			return pipeline_light_source;
		default:
			throw std::runtime_error("GraphicsEnginePipelineManager::get_pipeline: invalid pipeline type");
	};
}