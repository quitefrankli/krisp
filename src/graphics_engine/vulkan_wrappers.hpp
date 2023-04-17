#pragma once

#include <vulkan/vulkan.hpp>


// An attachment in the context of Vulkan is just a resource used during rendering
//	i.e. color, depth, stencil attachments
// Since this would also describe an image view another way of looking at frame buffer
// 	is that it is a collection of image views
struct RenderingAttachment
{
	// device memory object that holds raw pixel data
	VkDeviceMemory image_memory = VK_NULL_HANDLE;
	// interprets image_memory, represents image resource that can be used as texture or render target
	// it also defines the format of the image data
	VkImage image = VK_NULL_HANDLE;
	// Describes how one accesses the image, i.e. which part of the image to access
	VkImageView image_view = VK_NULL_HANDLE;

	void destroy(VkDevice device)
	{
		if (image_memory)
			vkFreeMemory(device, image_memory, nullptr);
		if (image)
			vkDestroyImage(device, image, nullptr);
		if (image_view)
			vkDestroyImageView(device, image_view, nullptr);
	}
};