#pragma once

#include "graphics_engine_base_module.hpp"
#include "graphics_engine_texture.hpp"
#include "utility.hpp"

#include <quill/Quill.h>

#include <unordered_map>
#include <string>


template<typename GraphicsEngineT>
class GraphicsEngineTextureManager : public GraphicsEngineBaseModule<GraphicsEngineT>
{
public:
	GraphicsEngineTextureManager(GraphicsEngineT& engine);
	~GraphicsEngineTextureManager();

	// automatically generates texture if requested texture does not exist
	GraphicsEngineTexture& fetch_texture(std::string_view texture_path, ETextureSamplerType sampler_type);

private:
	GraphicsEngineTexture create_texture(const std::string_view texture_path, ETextureSamplerType sampler_type);
	void create_texture_image(
		const std::string_view texture_path, 
		VkImage& texture_image,
		VkDeviceMemory& texture_image_memory);
	VkSampler create_texture_sampler(ETextureSamplerType sampler_type);
	void copy_buffer_to_image(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

private:
	// note that this is not deleted even when an object referencing this texture gets destroyed
	std::unordered_map<std::string, GraphicsEngineTexture> texture_units;
};