#pragma once

#include "graphics_engine_base_module.hpp"

#include <vulkan/vulkan.hpp>


class GraphicsEngine;

class GraphicsEngineTexture : public GraphicsEngineBaseModule
{
private:
	// while shaders can access pixel values in the buffer,
	// it's better to use Vk Image Objects as there are some optimisations due
	// to the 2D nature of textures
	VkImage texture_image;
	
	/*
		while it's possible for shaders to read texels directly off the image, it's better for the shaders to access the texels
		through a sampler which allows for filtering and transformations. Examples include:
		* Bilinear filtering
		* Anisotropic filtering
		* Repeat/clamp to edge/clamp to border
	*/
	VkSampler texture_sampler;
	VkImageView texture_image_view;
	VkDeviceMemory texture_image_memory;

	void create_texture_image();

	// handle layout transition so that image is in right layout
	void transition_image_layout(VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout);

	void copy_buffer_to_image(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

	void create_texture_image_view();

	void create_texture_sampler();

public:
	GraphicsEngineTexture(GraphicsEngine& graphics_engine);

	void init();

	void create_image(uint32_t width, 
					  uint32_t height, 
					  VkFormat format, 
					  VkImageTiling tiling,
					  VkImageUsageFlags usage,
					  VkMemoryPropertyFlags properties,
					  VkImage& image,
					  VkDeviceMemory& image_memory);

	VkImageView& get_texture_image() { return texture_image_view; }
	VkSampler& get_texture_sampler() { return texture_sampler; }

	void cleanup();
};