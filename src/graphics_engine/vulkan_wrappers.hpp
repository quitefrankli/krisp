#pragma once

#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>


// An attachment in the context of Vulkan is just a resource used during rendering
//	i.e. color, depth, stencil attachments
// Since this would also describe an image view another way of looking at frame buffer
// 	is that it is a collection of image views
struct RenderingAttachment
{
	// VMA allocation backing the image's pixel data.
	VmaAllocation allocation = nullptr;
	// represents an image resource that can be used as a texture or render target
	// it also defines the format of the image data
	VkImage image = VK_NULL_HANDLE;
	// Describes how one accesses the image, i.e. which part of the image to access
	VkImageView image_view = VK_NULL_HANDLE;

	void destroy(VkDevice device, VmaAllocator allocator)
	{
		if (image_view)
		{
			vkDestroyImageView(device, image_view, nullptr);
			image_view = VK_NULL_HANDLE;
		}
		if (image && allocation)
			vmaDestroyImage(allocator, image, allocation);
		image = VK_NULL_HANDLE;
		allocation = nullptr;
	}
};
