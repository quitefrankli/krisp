#include "pipeline_manager.ipp"
#include "pipeline.ipp"
#include "pipelines.ipp"

#include "game_engine.hpp"

using GraphicsEngineT = GameEngine<GraphicsEngine>::GraphicsEngineT;


template class GraphicsEnginePipelineManager<GraphicsEngineT>;
template class GraphicsEnginePipeline<GraphicsEngineT>;
template class StencilPipeline<GraphicsEngineT>;