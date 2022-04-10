#include "pipeline_manager.hpp"
#include "graphics_engine/graphics_engine.hpp"
#include "graphics_engine/graphics_engine_depth_buffer.hpp"

#include <cassert>
#include <iostream>


GraphicsEnginePipelineManager::GraphicsEnginePipelineManager(GraphicsEngine& engine) :
	GraphicsEngineBaseModule(engine)
{
	// pipeline is not copyable and initializer list requires copyable entries
	const auto add_pipeline = [&](ERenderType type)
	{
		pipelines.emplace(type, GraphicsEnginePipeline(engine, type));
	};
	
	add_pipeline(ERenderType::STANDARD);
	add_pipeline(ERenderType::COLOR);
	add_pipeline(ERenderType::WIREFRAME);
	add_pipeline(ERenderType::LIGHT_SOURCE);
}

GraphicsEnginePipelineManager::~GraphicsEnginePipelineManager()
{
}

GraphicsEnginePipeline& GraphicsEnginePipelineManager::get_pipeline(ERenderType type)
{
	auto it = pipelines.find(type);
	if (it == pipelines.end())
	{
		throw std::runtime_error("GraphicsEnginePipelineManager::get_pipeline: invalid pipeline type");
	}

	return it->second;
}