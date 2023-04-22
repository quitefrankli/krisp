#pragma once

#include "graphics_engine_texture.hpp"
#include "graphics_engine.hpp"
#include "utility.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <quill/Quill.h>


GraphicsEngineTexture::GraphicsEngineTexture(
	VkImage texture_image,
	VkDeviceMemory texture_image_memory,
	VkImageView texture_image_view,
	VkSampler texture_sampler,
	const glm::uvec3& dimensions) :
	texture_image(texture_image),
	texture_image_memory(texture_image_memory),
	texture_image_view(texture_image_view),
	texture_sampler(texture_sampler),
	dimensions(dimensions)
{
}

GraphicsEngineTexture::GraphicsEngineTexture(GraphicsEngineTexture&& other) noexcept :
	texture_image(other.texture_image),
	texture_sampler(other.texture_sampler),
	texture_image_view(other.texture_image_view),
	texture_image_memory(other.texture_image_memory),
	dimensions(other.dimensions)
{
	other.texture_sampler = nullptr;
	other.texture_image_view = nullptr;
	other.texture_image = nullptr;
	other.texture_image_memory = nullptr;
}

GraphicsEngineTexture::~GraphicsEngineTexture()
{
	if (texture_image_view || texture_image || texture_image_memory)
	{
		LOG_ERROR(Utility::get().get_logger(), "GraphicsEngineTexture: texture was not destroyed!");
		throw std::runtime_error("GraphicsEngineTexture: texture was not destroyed!");
	}
}

void GraphicsEngineTexture::destroy(VkDevice device)
{
	// note we destroy the view before the actual image
	if (texture_image_view)
	{
		vkDestroyImageView(device, texture_image_view, nullptr); 
		texture_image_view = nullptr;
	}
	if (texture_image)
	{
		vkDestroyImage(device, texture_image, nullptr);
		texture_image = nullptr;
	}
	if (texture_image_memory)
	{
		vkFreeMemory(device, texture_image_memory, nullptr);
		texture_image_memory = nullptr;
	}
}