#pragma once

#include "graphics_engine_base_module.hpp"
#include "graphics_engine_texture.hpp"

#include <unordered_map>
#include <string>


class GraphicsEngineTextureManager : public GraphicsEngineBaseModule
{
public:
	GraphicsEngineTextureManager(GraphicsEngine& engine);

	GraphicsEngineTexture& create_new_unit(std::string texture_path);

	// an image is an actual piece of data memory, similar to a buffer
	void create_image(uint32_t width, 
					  uint32_t height, 
					  VkFormat format, 
					  VkImageTiling tiling,
					  VkImageUsageFlags usage,
					  VkMemoryPropertyFlags properties,
					  VkImage& image,
					  VkDeviceMemory& image_memory);

	// an image view is just a view of an image, it does not mutate the image
	// i.e. string_view vs string
	VkImageView create_image_view(VkImage& image,
								  VkFormat format,
						   		  VkImageAspectFlags aspect_flags,
								  VkImageViewType view_type = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D);

	// a sampler is essentially a functor that allows one to interpret an imageview

private:
	// note that this is not deleted even when an object referencing this texture gets destroyed
	std::unordered_map<std::string, GraphicsEngineTexture> texture_units;
};