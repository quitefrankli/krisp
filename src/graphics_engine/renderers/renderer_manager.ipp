#include "renderer_manager.hpp"


template<typename GraphicsEngineT>
RendererManager<GraphicsEngineT>::RendererManager(GraphicsEngineT& engine) :
	GraphicsEngineBaseModule<GraphicsEngineT>(engine)
{
	renderers[ERendererType::RASTERIZATION] = std::make_unique<RasterizationRenderer<GraphicsEngineT>>(engine);
	renderers[ERendererType::RAYTRACING] = std::make_unique<RaytracingRenderer<GraphicsEngineT>>(engine);
	renderers[ERendererType::GUI] = std::make_unique<GuiRenderer<GraphicsEngineT>>(engine);
	renderers[ERendererType::OFFSCREEN_GUI_VIEWPORT] = std::make_unique<OffscreenGuiViewportRenderer<GraphicsEngineT>>(engine);
	renderers[ERendererType::SHADOW_MAP] = std::make_unique<ShadowMapRenderer<GraphicsEngineT>>(engine);
	renderers[ERendererType::QUAD] = std::make_unique<QuadRenderer<GraphicsEngineT>>(engine);
}

template<typename GraphicsEngineT>
void RendererManager<GraphicsEngineT>::pipe_output_to_quad_renderer(ERendererType src_renderer)
{
	if (src_renderer == ERendererType::QUAD)
	{
		throw std::runtime_error("Cannot pipe output to quad renderer from quad renderer");
	}

	auto& quad_renderer = static_cast<QuadRenderer<GraphicsEngineT>&>(get_renderer(ERendererType::QUAD));

	if (src_renderer == ERendererType::NONE)
	{
		quad_renderer.set_texture(nullptr, nullptr);
		return;
	}

	auto& src = get_renderer(src_renderer);

	quad_renderer.set_texture(
		src.get_output_image_view(0),
		get_graphics_engine().get_texture_mgr().fetch_sampler(ETextureSamplerType::ADDR_MODE_CLAMP_TO_EDGE));
	quad_renderer.set_texture_sampling_flags(1);
}
