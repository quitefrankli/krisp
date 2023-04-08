#include "renderer.hpp"
#include "renderers.ipp"
#include "renderer_manager.ipp"
#include "graphics_engine/graphics_engine.hpp"
#include "game_engine.hpp"


using GraphicsEngineT = GameEngine<GraphicsEngine>::GraphicsEngineT;

template<typename GraphicsEngineT>
Renderer<GraphicsEngineT>::Renderer(GraphicsEngineT& engine) :
	GraphicsEngineBaseModule<GraphicsEngineT>(engine)
{
}

template<typename GraphicsEngineT>
Renderer<GraphicsEngineT>::~Renderer()
{
	auto logical_device = get_graphics_engine().get_logical_device();
	vkDestroyRenderPass(logical_device, render_pass, nullptr);
	for (auto frame_buffer : frame_buffers)
	{
		vkDestroyFramebuffer(logical_device, frame_buffer, nullptr);
	}
}

template<typename GraphicsEngineT>
constexpr uint32_t Renderer<GraphicsEngineT>::get_num_inflight_frames()
{
	return GraphicsEngineT::get_num_swapchain_images();	
}

template class Renderer<GraphicsEngineT>;
template class RasterizationRenderer<GraphicsEngineT>;
// template class RaytracingRenderer<GraphicsEngineT>;
// template class GUIRenderer<GraphicsEngineT>;
template class RendererManager<GraphicsEngineT>;