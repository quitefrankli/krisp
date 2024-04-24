#include "renderer_manager.hpp"


RendererManager::RendererManager(GraphicsEngine& engine) :
	GraphicsEngineBaseModule(engine)
{
	renderers[ERendererType::RASTERIZATION] = std::make_unique<RasterizationRenderer>(engine);
	renderers[ERendererType::RAYTRACING] = std::make_unique<RaytracingRenderer>(engine);
	renderers[ERendererType::GUI] = std::make_unique<GuiRenderer>(engine);
	renderers[ERendererType::OFFSCREEN_GUI_VIEWPORT] = std::make_unique<OffscreenGuiViewportRenderer>(engine);
	renderers[ERendererType::SHADOW_MAP] = std::make_unique<ShadowMapRenderer>(engine);
	renderers[ERendererType::QUAD] = std::make_unique<QuadRenderer>(engine);
}

void RendererManager::linkup_renderers()
{
	// link output of shadowmap renderer to input of rasterization renderer
	auto& shadow_map_renderer = get_renderer(ERendererType::SHADOW_MAP);
	std::vector<VkImageView> shadow_map_inputs;
	for (int frame_idx = 0; frame_idx < CSTS::NUM_EXPECTED_SWAPCHAIN_IMAGES; ++frame_idx)
	{
		shadow_map_inputs.push_back(get_renderer(ERendererType::SHADOW_MAP).get_output_image_view(frame_idx));
	}	
	auto& rasterization_renderer = static_cast<RasterizationRenderer&>(get_renderer(ERendererType::RASTERIZATION));
	rasterization_renderer.set_shadow_map_inputs(shadow_map_inputs);
}

void RendererManager::pipe_output_to_quad_renderer(ERendererType src_renderer)
{
	if (src_renderer == ERendererType::QUAD)
	{
		throw std::runtime_error("Cannot pipe output to quad renderer from quad renderer");
	}

	auto& quad_renderer = static_cast<QuadRenderer&>(get_renderer(ERendererType::QUAD));

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
