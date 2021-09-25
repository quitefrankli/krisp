// image view is a view into an image, it describes how to access the image
// and which part of the image to access
// it's essentially a 2D texture depth texture without any mipmapping levels

#include "graphics_engine.hpp"

#include <GLFW/glfw3.h>


void GraphicsEngine::create_image_views()
{
	swap_chain_image_views.resize(swap_chain_images.size());
	
	for (int i = 0; i < swap_chain_images.size(); i++)
	{
		VkImageViewCreateInfo create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		create_info.image = swap_chain_images[i];
		create_info.viewType = VK_IMAGE_VIEW_TYPE_2D; // specifies how the image data should be interpreted
														// i.e. treat images as 1D, 2D, 3D textures and cube maps
		create_info.format = swap_chain_image_format;
		create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; // describes image purpose and which part should be accessed
		create_info.subresourceRange.baseMipLevel = 0;
		create_info.subresourceRange.levelCount = 1;
		create_info.subresourceRange.baseArrayLayer = 0;
		create_info.subresourceRange.layerCount = 1;

		if (vkCreateImageView(get_logical_device(), &create_info, nullptr, &swap_chain_image_views[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create image views!");
		}
	}
}
