#include "renderer_manager.hpp"


template<typename GraphicsEngineT>
RendererManager<GraphicsEngineT>::RendererManager(GraphicsEngineT& engine)
{
	renderers[ERendererType::RASTERIZATION] = std::make_unique<RasterizationRenderer<GraphicsEngineT>>(engine);
	renderers[ERendererType::RAYTRACING] = std::make_unique<RaytracingRenderer<GraphicsEngineT>>(engine);
	renderers[ERendererType::GUI] = std::make_unique<GuiRenderer<GraphicsEngineT>>(engine);
}
