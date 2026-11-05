#pragma once

#include <vulkan/vulkan.h>

struct RenderingFormats
{
	VkFormat color = VK_FORMAT_UNDEFINED;
	VkFormat depth = VK_FORMAT_UNDEFINED;
	VkFormat stencil = VK_FORMAT_UNDEFINED;
};

// Dynamic rendering leaves attachment transitions and visibility to the caller.
inline void command_image_barrier(
	VkCommandBuffer command_buffer, VkImage image, VkImageAspectFlags aspects,
	VkImageLayout old_layout, VkImageLayout new_layout,
	VkPipelineStageFlags source_stage, VkAccessFlags source_access,
	VkPipelineStageFlags destination_stage, VkAccessFlags destination_access,
	uint32_t layers = 1)
{
	VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
	barrier.srcAccessMask = source_access;
	barrier.dstAccessMask = destination_access;
	barrier.oldLayout = old_layout;
	barrier.newLayout = new_layout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange = {aspects, 0, 1, 0, layers};
	vkCmdPipelineBarrier(command_buffer, source_stage, destination_stage, 0,
		0, nullptr, 0, nullptr, 1, &barrier);
}
