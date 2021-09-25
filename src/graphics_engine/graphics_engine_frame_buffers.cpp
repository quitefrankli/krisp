#include "graphics_engine.hpp"

void GraphicsEngine::create_frame_buffers()
{
	swap_chain_frame_buffers.resize(swap_chain_image_views.size());
	for (int i = 0; i < swap_chain_image_views.size(); i++)
	{
		VkImageView attachments[] = { swap_chain_image_views[i] };
		VkFramebufferCreateInfo frame_buffer_create_info{};
		frame_buffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		frame_buffer_create_info.renderPass = render_pass;
		frame_buffer_create_info.attachmentCount = 1;
		frame_buffer_create_info.pAttachments = attachments;
		frame_buffer_create_info.width = swap_chain_extent.width;
		frame_buffer_create_info.height = swap_chain_extent.height;
		frame_buffer_create_info.layers = 1;

		if (vkCreateFramebuffer(get_logical_device(), &frame_buffer_create_info, nullptr, &swap_chain_frame_buffers[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create framebuffer!");
		}
	}
}