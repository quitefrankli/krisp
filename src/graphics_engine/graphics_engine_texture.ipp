#pragma once

#include "graphics_engine_texture.hpp"
#include "graphics_engine.hpp"
#include "utility.hpp"

#include <quill/LogMacros.h>


GraphicsEngineTexture::GraphicsEngineTexture(
	VkImage texture_image,
	VmaAllocation allocation,
	VkImageView texture_image_view,
	VkSampler texture_sampler,
	const glm::uvec3& dimensions) :
	texture_image(texture_image),
	allocation(allocation),
	texture_image_view(texture_image_view),
	texture_sampler(texture_sampler),
	dimensions(dimensions)
{
}

GraphicsEngineTexture::GraphicsEngineTexture(GraphicsEngineTexture&& other) noexcept :
	texture_image(other.texture_image),
	texture_sampler(other.texture_sampler),
	texture_image_view(other.texture_image_view),
	allocation(other.allocation),
	dimensions(other.dimensions)
{
	other.texture_sampler = nullptr;
	other.texture_image_view = nullptr;
	other.texture_image = nullptr;
	other.allocation = nullptr;
}

GraphicsEngineTexture::~GraphicsEngineTexture()
{
	if (texture_image_view || texture_image || allocation)
	{
		LOG_ERROR(Utility::get_logger(), "GraphicsEngineTexture: texture was not destroyed!");
		assert(false);
	}
}

void GraphicsEngineTexture::destroy(VkDevice device, VmaAllocator allocator)
{
	// note we destroy the view before the actual image
	if (texture_image_view)
	{
		vkDestroyImageView(device, texture_image_view, nullptr); 
		texture_image_view = nullptr;
	}
	if (texture_image && allocation)
		vmaDestroyImage(allocator, texture_image, allocation);
	texture_image = VK_NULL_HANDLE;
	allocation = nullptr;
}
