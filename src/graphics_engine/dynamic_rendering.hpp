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
	VkPipelineStageFlags2 source_stage, VkAccessFlags2 source_access,
	VkPipelineStageFlags2 destination_stage, VkAccessFlags2 destination_access,
	uint32_t layers = 1, uint32_t mip_levels = 1)
{
	VkImageMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
	barrier.srcStageMask = source_stage;
	barrier.srcAccessMask = source_access;
	barrier.dstStageMask = destination_stage;
	barrier.dstAccessMask = destination_access;
	barrier.oldLayout = old_layout;
	barrier.newLayout = new_layout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange = {aspects, 0, mip_levels, 0, layers};
	VkDependencyInfo dependency_info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
	dependency_info.imageMemoryBarrierCount = 1;
	dependency_info.pImageMemoryBarriers = &barrier;
	vkCmdPipelineBarrier2(command_buffer, &dependency_info);
}
