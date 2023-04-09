#pragma once

#include "graphics_engine_texture.hpp"
#include "graphics_engine.hpp"
#include "utility_functions.hpp"
#include "graphics_engine_texture_manager.hpp"
#include "utility.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <quill/Quill.h>


template<typename GraphicsEngineT>
GraphicsEngineTexture<GraphicsEngineT>::GraphicsEngineTexture(GraphicsEngineTextureManager<GraphicsEngineT>& manager_, std::string texture_path) :
	manager(manager_), GraphicsEngineBaseModule<GraphicsEngineT>(manager_.get_graphics_engine())
{
	create_texture_image(texture_path);
	texture_image_view = get_graphics_engine().create_image_view(texture_image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
	create_texture_sampler();
}

template<typename GraphicsEngineT>
GraphicsEngineTexture<GraphicsEngineT>::GraphicsEngineTexture(GraphicsEngineTexture<GraphicsEngineT>&& other) noexcept :
	GraphicsEngineBaseModule<GraphicsEngineT>(other.get_graphics_engine()), 
	manager(other.manager),
	texture_image(std::move(other.texture_image)),
	texture_sampler(std::move(other.texture_sampler)),
	texture_image_view(std::move(other.texture_image_view)),
	texture_image_memory(std::move(other.texture_image_memory))
{
	other.require_cleanup = false;
}

template<typename GraphicsEngineT>
GraphicsEngineTexture<GraphicsEngineT>::~GraphicsEngineTexture()
{
	if (!require_cleanup)
	{
		return;
	}
	
	vkDestroySampler(get_logical_device(), texture_sampler, nullptr);
	vkDestroyImageView(get_logical_device(), texture_image_view, nullptr); // note we destroy the view before the actual image
	vkDestroyImage(get_logical_device(), texture_image, nullptr);
	vkFreeMemory(get_logical_device(), texture_image_memory, nullptr);
}

// load image and upload it into a vulkan image object
template<typename GraphicsEngineT>
void GraphicsEngineTexture<GraphicsEngineT>::create_texture_image(std::string texture_path)
{
	int width, height, channels;
	std::unique_ptr<stbi_uc, std::function<void(stbi_uc*)>> pixels( 
		stbi_load(texture_path.c_str(), &width, &height, &channels, STBI_rgb_alpha),

		// custom unique_ptr destructor
		[](stbi_uc* ptr) 
		{ 
			stbi_image_free(ptr); 
		}
	);

	// VkDeviceSize size = width * height * channels; // for some reason channels = 3?
	VkDeviceSize size = width * height * 4;
	if (!pixels.get())
	{
		throw std::runtime_error(std::string("failed to load texture image! ") + texture_path);
	}

	VkBuffer staging_buffer;
	VkDeviceMemory staging_buffer_memory;

	get_graphics_engine().create_buffer(size, 
				  VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
				  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				  staging_buffer,
				  staging_buffer_memory);

	void* data;
	vkMapMemory(get_logical_device(), staging_buffer_memory, 0, size, 0, &data);
	memcpy(data, pixels.get(), static_cast<size_t>(size));
	vkUnmapMemory(get_logical_device(), staging_buffer_memory);

	get_graphics_engine().create_image(width, 
				 height, 
				 VK_FORMAT_R8G8B8A8_SRGB, // we may want to reconsider SRGB
				 VK_IMAGE_TILING_OPTIMAL,
				 VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, // we want to use it as dest and be able to access it from shader to colour the mesh
				 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				 texture_image,
				 texture_image_memory);

	// copy the staging buffer to the texture image,
	// undefined image layout works because we don't care about the contents before performing copy
	transition_image_layout(texture_image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	copy_buffer_to_image(staging_buffer, texture_image, static_cast<uint32_t>(width), static_cast<uint32_t>(height));

	// transition one more time for shader access
	transition_image_layout(texture_image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	vkDestroyBuffer(get_logical_device(), staging_buffer, nullptr);
	vkFreeMemory(get_logical_device(), staging_buffer_memory, nullptr);

	LOG_INFO(Utility::get().get_logger(), 
			 "GraphicsEngineTexture::create_texture_image: created texture from:={}", 
			 texture_path);
}

// handle layout transition so that image is in right layout
template<typename GraphicsEngineT>
void GraphicsEngineTexture<GraphicsEngineT>::transition_image_layout(VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout)
{
	VkCommandBuffer command_buffer = get_graphics_engine().begin_single_time_commands();

	// pipeline barrier to synchronize access to resources
	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER; 
	barrier.oldLayout = old_layout;
	barrier.newLayout = new_layout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image; // specifies image affeced
	// subresourceRange specifies what part of the image is affected
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0; // image is an array with no mip mapping
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = 0;

	// transition types:
	//  * undefined -> transfer destination: transfer writes that don't need to wait on anything
	//  * transfer destination -> shader reading: shader reads should wait on transfer writes
	//		specifically the shader reads in the fragment shader
	const auto check_transition = [old_layout, new_layout](VkImageLayout x, VkImageLayout y) -> bool
	{
		return old_layout == x && new_layout == y;
	};
	VkPipelineStageFlags sourceStage, destinationStage;
	if (check_transition(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)) 
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	} else if (check_transition(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)) 
	{
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	} else if (check_transition(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL))
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = 0;
		sourceStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
		destinationStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	} else {
		throw std::invalid_argument("unsupported layout transition!");
	}

	vkCmdPipelineBarrier(
		command_buffer,
		sourceStage, // which pipeline stage the operation should occur before the barrier
		destinationStage, // pipeline stage in which the operation will wait on the barrier
		0, // 
		0,
		nullptr,
		0,
		nullptr,
		1,
		&barrier
	);

	get_graphics_engine().end_single_time_commands(command_buffer);
}

template<typename GraphicsEngineT>
void GraphicsEngineTexture<GraphicsEngineT>::copy_buffer_to_image(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
{
	VkCommandBuffer command_buffer = get_graphics_engine().begin_single_time_commands();

	VkBufferImageCopy region{};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;

	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;

	region.imageOffset = {0, 0, 0};
	region.imageExtent = {
		width,
		height,
		1
	};

	vkCmdCopyBufferToImage(
		command_buffer,
		buffer,
		image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&region
	);

	get_graphics_engine().end_single_time_commands(command_buffer);
}

template<typename GraphicsEngineT>
void GraphicsEngineTexture<GraphicsEngineT>::create_texture_sampler()
{
	VkSamplerCreateInfo sampler_info{};
	sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler_info.magFilter = VK_FILTER_LINEAR; // how to interpolate texels that are magnified, solves oversampling
	sampler_info.minFilter = VK_FILTER_LINEAR; // how to interpolate texels that are minimised, solves undersampling
	// U,V,W is convention for texture space dimensions
	sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.anisotropyEnable = true; // small performance hiccup
	sampler_info.maxAnisotropy = get_graphics_engine().get_device_module().
		get_physical_device_properties().properties.limits.maxSamplerAnisotropy; // higher = slower
	sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	sampler_info.unnormalizedCoordinates = false; // specifies coordinate system to address texels, in real world this is always true
												  // so that you can use textures of varying resolutions with same coordinates
	sampler_info.compareEnable = false; // if enabled, texels will first be compared to a value and the result of comparison is used in filtering
	sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
	sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR; //
	sampler_info.mipLodBias = 0.0f;
	sampler_info.minLod = 0.0f;
	sampler_info.maxLod = 0.0f;

	if (vkCreateSampler(get_logical_device(), &sampler_info, nullptr, &texture_sampler) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create texture sampler!");
	}
}