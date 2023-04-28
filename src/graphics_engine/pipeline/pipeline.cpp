#include "pipeline_manager.ipp"
#include "pipeline.ipp"
#include "pipelines.ipp"

#include "game_engine.hpp"

using GraphicsEngineT = GameEngine<GraphicsEngine>::GraphicsEngineT;


template class GraphicsEnginePipelineManager<GraphicsEngineT>;
template class GraphicsEnginePipeline<GraphicsEngineT>;
template class StencilPipeline<GraphicsEngineT>;
template class WireframePipeline<GraphicsEngineT>;

template VkVertexInputBindingDescription WireframePipeline<GraphicsEngineT>::_get_binding_description<SDS::ColorVertex>();
template VkVertexInputBindingDescription WireframePipeline<GraphicsEngineT>::_get_binding_description<SDS::TexVertex>();
template std::vector<VkVertexInputAttributeDescription> WireframePipeline<GraphicsEngineT>::_get_attribute_descriptions<SDS::ColorVertex>();
template std::vector<VkVertexInputAttributeDescription> WireframePipeline<GraphicsEngineT>::_get_attribute_descriptions<SDS::TexVertex>();