#pragma once

#include "graphics_engine.hpp"
#include "objects/object.hpp"


template<typename GameEngineT>
void GraphicsEngine<GameEngineT>::copy_buffer(VkBuffer src_buffer, VkBuffer dest_buffer, size_t size)
{
	// memory transfer operations are executed using command buffers

	VkCommandBuffer command_buffer = begin_single_time_commands();

	// actual copy command
	VkBufferCopy copy_region{};
	copy_region.size = size;
	vkCmdCopyBuffer(command_buffer, src_buffer, dest_buffer, 1, &copy_region);

	end_single_time_commands(command_buffer);
}

template<typename GameEngineT>
RenderingAttachment GraphicsEngine<GameEngineT>::create_depth_buffer_attachment()
{
	const VkFormat depth_format = find_depth_format();
	const auto extent = get_extent();
	RenderingAttachment depth_attachment;
	create_image(
		extent.width,
		extent.height,
		depth_format,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		depth_attachment.image,
		depth_attachment.image_memory,
		get_msaa_samples());
	depth_attachment.image_view = create_image_view(
		depth_attachment.image, 
		depth_format, 
		VK_IMAGE_ASPECT_DEPTH_BIT);

	return depth_attachment;
}

template<typename GameEngineT>
VkFormat GraphicsEngine<GameEngineT>::find_depth_format()
{
	if (!depth_format)
	{
		const auto find_supported_format = [&](
			std::vector<VkFormat> candidates, 
			VkImageTiling tiling, 
			VkFormatFeatureFlags features)
		{
			for (VkFormat format : candidates)
			{
				VkFormatProperties props;
				vkGetPhysicalDeviceFormatProperties(get_physical_device(), format, &props);
				if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
				{
					return format;
				}

				if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
				{
					return format;
				}
			}

			throw std::runtime_error("failed to find supported format!");
		};

		depth_format = find_supported_format(
			{VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
	}

	return depth_format.value();
}
