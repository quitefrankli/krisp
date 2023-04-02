#pragma once

#include "graphics_engine/graphics_engine_base_module.hpp"

#include <vulkan/vulkan.hpp>


// TODO: move to separate file
struct Attachment
{
	VkImage image;
	VkImageView image_view;
	VkDeviceMemory image_memory;
};

// TODO: make this inherit from RenderPass
template<typename GraphicsEngineT>
class OffScreenRenderPass : public GraphicsEngineBaseModule<GraphicsEngineT>
{
public:
	OffScreenRenderPass(GraphicsEngineT& engine);
	virtual ~OffScreenRenderPass() override;
	void draw();
	Attachment& get_color_attachment() { return color_attachment; }
	Attachment& get_depth_attachment() { return depth_attachment; }

private:
	// TODO: move to base class
	VkRenderPass create_render_pass(
		const std::vector<VkFormat>& colorAttachmentFormats,
		VkFormat depthAttachmentFormat,
		VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		VkImageLayout finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	void initialise_render_pass();

private:
	Attachment color_attachment;
	Attachment depth_attachment;
	VkFramebuffer frame_buffer;
	VkRenderPass render_pass;
};