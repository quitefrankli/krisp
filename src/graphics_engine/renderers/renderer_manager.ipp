#include "renderer_manager.hpp"


RendererManager::RendererManager(GraphicsEngine& engine) :
	GraphicsEngineBaseModule(engine)
{
	renderers[ERendererType::RASTERIZATION] = std::make_unique<RasterizationRenderer>(engine);
	// renderers[ERendererType::RAYTRACING] = std::make_unique<RaytracingRenderer>(engine);
	renderers[ERendererType::GUI] = std::make_unique<GuiRenderer>(engine);
	renderers[ERendererType::SHADOW_MAP] = std::make_unique<ShadowMapRenderer>(engine);
	renderers[ERendererType::QUAD] = std::make_unique<QuadRenderer>(engine);
	renderers[ERendererType::PRESENTATION] = std::make_unique<PresentationRenderer>(engine);
	renderers[ERendererType::PARTICLE] = std::make_unique<ParticleRenderer>(engine);
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

	quad_renderer.set_renderer_source(
		src_renderer,
		get_graphics_engine().get_texture_mgr().fetch_sampler(
			PbrMaterial::TextureSampler::clamp_to_edge()));
	quad_renderer.set_texture_sampling_flags(1);
}
