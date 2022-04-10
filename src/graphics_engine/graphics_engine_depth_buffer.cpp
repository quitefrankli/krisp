#include "graphics_engine_depth_buffer.hpp"

#include "graphics_engine_texture.hpp"
#include "graphics_engine.hpp"

GraphicsEngineDepthBuffer::GraphicsEngineDepthBuffer(GraphicsEngine& engine) :
	GraphicsEngineBaseModule(engine)
{
	VkFormat depth_format = findDepthFormat(get_physical_device());
	auto extent = get_graphics_engine().get_extent_unsafe();
	get_graphics_engine().get_texture_mgr().create_image(extent.width,
												   extent.height,
												   depth_format,
												   VK_IMAGE_TILING_OPTIMAL,
												   VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
												   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
												   image,
												   memory);

	view = get_graphics_engine().get_texture_mgr().create_image_view(image, depth_format, VK_IMAGE_ASPECT_DEPTH_BIT);
}

GraphicsEngineDepthBuffer::~GraphicsEngineDepthBuffer()
{
	vkDestroyImage(get_logical_device(), image, nullptr);
	vkDestroyImageView(get_logical_device(), view, nullptr);
	vkFreeMemory(get_logical_device(), memory, nullptr);
}

VkFormat GraphicsEngineDepthBuffer::find_supported_format(VkPhysicalDevice device, std::vector<VkFormat> candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
{
	for (auto& format : candidates)
	{
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(device, format, &props);

		if (tiling == VK_IMAGE_TILING_LINEAR && props.linearTilingFeatures & features == features)
		{
			return format;
		} else if (tiling == VK_IMAGE_TILING_OPTIMAL && props.optimalTilingFeatures & features == features)
		{
			return format;
		}
	}

	throw std::runtime_error("failed to find supported format!");
}

VkFormat GraphicsEngineDepthBuffer::findDepthFormat(VkPhysicalDevice device) {
    return find_supported_format(
		device,
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

bool GraphicsEngineDepthBuffer::hasStencilComponent(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}