#pragma once

#include "graphics_engine/graphics_engine_base_module.hpp"
#include "graphics_engine/vulkan_wrappers.hpp"

#include <vulkan/vulkan.hpp>


enum class ERendererType
{
	RASTERIZATION,
	RAYTRACING,
	GUI
};

// A renderer is simply anything that submits draw commands and fills up a command buffer
// Each renderer can only have ONE renderpass and each renderpass must be unique to the renderer
template<typename GraphicsEngineT>
class Renderer : public GraphicsEngineBaseModule<GraphicsEngineT>
{
public:
	Renderer(GraphicsEngineT& engine);
	~Renderer();

	// generates framebuffers
	virtual void allocate_inflight_frame_resources(VkImage presentation_image) = 0;
	virtual void submit_draw_commands(VkCommandBuffer command_buffer, uint32_t frame_index) = 0;
	virtual constexpr ERendererType get_renderer_type() const = 0;

	VkRenderPass get_render_pass() { return render_pass; }

protected:
	static constexpr uint32_t get_num_inflight_frames();

protected:
	// A render pass is a general description of steps to draw something on the screen
	//	it's made of at least 1 subpass, which can be executed in parallel (subpasses are mostly used in mobile optimisations)
	//	and also sequentially fed into each other (however this doesnt mean it makes
	//	for a good processor as it can not sample surrounding pixels https://www.saschawillems.de/blog/2018/07/19/vulkan-input-attachments-and-sub-passes/)
	// A render pass when provided a framebuffer will render into said buffer
	// Multiple render passes would be used for post processing effects
	VkRenderPass render_pass;

	// per frame resources
	std::vector<VkFramebuffer> frame_buffers;
};