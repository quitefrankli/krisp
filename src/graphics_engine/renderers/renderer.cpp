#include "renderer.hpp"
#include "rasterization_renderer.ipp"
#include "gui_renderer.ipp"
#include "raytracing_renderer.ipp"
#include "offscreen_gui_viewport_renderer.ipp"
#include "shadowmap_renderer.ipp"
#include "quad_renderer.ipp"
#include "renderer_manager.ipp"
#include "graphics_engine/graphics_engine.hpp"
#include "game_engine.hpp"


Renderer::Renderer(GraphicsEngine& engine) :
	GraphicsEngineBaseModule(engine)
{
}

Renderer::~Renderer()
{
	auto logical_device = this->get_graphics_engine().get_logical_device();
	if (render_pass)
	{
		vkDestroyRenderPass(logical_device, render_pass, nullptr);
	}
	for (auto frame_buffer : frame_buffers)
	{
		vkDestroyFramebuffer(logical_device, frame_buffer, nullptr);
	}
}

VkExtent2D Renderer::get_extent()
{
	return this->get_graphics_engine().get_extent();
}

constexpr uint32_t Renderer::get_num_inflight_frames()
{
	return GraphicsEngine::get_num_swapchain_images();	
}