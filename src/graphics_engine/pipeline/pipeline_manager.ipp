#pragma once

#include "pipeline_manager.hpp"
#include "graphics_engine/graphics_engine.hpp"

#include <cassert>
#include <iostream>


template<typename GraphicsEngineT>
GraphicsEnginePipelineManager<GraphicsEngineT>::GraphicsEnginePipelineManager(GraphicsEngineT& engine) :
	GraphicsEngineBaseModule<GraphicsEngineT>(engine)
{
	// pipeline is not copyable and initializer list requires copyable entries
	const auto add_pipeline = [&](EPipelineType type)
	{
		pipelines.emplace(type, PipelineType::create_pipeline(engine, type));
	};
	
	add_pipeline(EPipelineType::STANDARD);
	add_pipeline(EPipelineType::COLOR);
	// add_pipeline(EPipelineType::WIREFRAME); // INTENTIONALLY OMITTED
	add_pipeline(EPipelineType::CUBEMAP);
	// add_pipeline(EPipelineType::STENCIL); // INTENTIONALLY OMITTED
	add_pipeline(EPipelineType::RAYTRACING);
	add_pipeline(EPipelineType::LIGHTWEIGHT_OFFSCREEN_PIPELINE);

	add_pipeline(EPipelineType::_STENCIL_COLOR_VERTICES);
	add_pipeline(EPipelineType::_STENCIL_TEXTURE_VERTICES);
	add_pipeline(EPipelineType::_WIREFRAME_COLOR_VERTICES);
	add_pipeline(EPipelineType::_WIREFRAME_TEXTURE_VERTICES);
}

template<typename GraphicsEngineT>
GraphicsEnginePipelineManager<GraphicsEngineT>::~GraphicsEnginePipelineManager()
{
}

template<typename GraphicsEngineT>
GraphicsEnginePipeline<GraphicsEngineT>& GraphicsEnginePipelineManager<GraphicsEngineT>::get_pipeline(EPipelineType type)
{
	auto it = pipelines.find(type);
	if (it == pipelines.end())
	{
		throw std::runtime_error("GraphicsEnginePipelineManager::get_pipeline: invalid pipeline type");
	}

	return *(it->second);
}