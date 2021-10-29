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

	void create_image(uint32_t width, 
					  uint32_t height, 
					  VkFormat format, 
					  VkImageTiling tiling,
					  VkImageUsageFlags usage,
					  VkMemoryPropertyFlags properties,
					  VkImage& image,
					  VkDeviceMemory& image_memory);

	VkImageView create_image_view(VkImage& image,
								VkFormat format,
						   		VkImageAspectFlags aspect_flags);

private:
	// note that this is not deleted even when an object referencing this texture gets destroyed
	std::unordered_map<std::string, GraphicsEngineTexture> texture_units;
};