#pragma once

#include "graphics_engine_texture_manager.hpp"


template<typename GraphicsEngineT>
GraphicsEngineTextureManager<GraphicsEngineT>::GraphicsEngineTextureManager(GraphicsEngineT& engine) : 
	GraphicsEngineBaseModule<GraphicsEngineT>(engine)
{
	LOG_INFO(
		Utility::get().get_logger(),
		"GraphicsEngineTextureManager: chosen max anisotropy:={}",
		engine.get_device_module().get_physical_device_properties().properties.limits.maxSamplerAnisotropy);
}

template<typename GraphicsEngineT>
GraphicsEngineTextureManager<GraphicsEngineT>::~GraphicsEngineTextureManager()
{
	for (auto& [texture_path, texture_unit] : texture_units)
	{
		texture_unit.destroy(get_logical_device());
	}

	for (auto& [sampler_type, sampler] : samplers)
	{
		vkDestroySampler(get_logical_device(), sampler, nullptr);
	}
}

template<typename GraphicsEngineT>
GraphicsEngineTexture& GraphicsEngineTextureManager<GraphicsEngineT>::fetch_texture(
	std::string_view texture_path,
	ETextureSamplerType sampler_type)
{
	auto it = texture_units.find(texture_path.data());
	if (it != texture_units.end())
	{
		return it->second;
	}

	return texture_units.emplace(texture_path, create_texture(texture_path, sampler_type)).first->second;
}

template<typename GraphicsEngineT>
VkSampler GraphicsEngineTextureManager<GraphicsEngineT>::fetch_sampler(ETextureSamplerType sampler_type)
{
	auto it = samplers.find(sampler_type);
	if (it != samplers.end())
	{
		return it->second;
	}

	return samplers.emplace(sampler_type, create_texture_sampler(sampler_type)).first->second;
}

template<typename GraphicsEngineT>
GraphicsEngineTexture GraphicsEngineTextureManager<GraphicsEngineT>::create_texture(
	const std::string_view texture_path,
	ETextureSamplerType sampler_type)
{
	VkImage texture_image;
	VkDeviceMemory texture_image_memory;
	const auto dim = create_texture_image(texture_path, texture_image, texture_image_memory);
	VkImageView texture_image_view = get_graphics_engine().create_image_view(
		texture_image, 
		VK_FORMAT_R8G8B8A8_SRGB, 
		VK_IMAGE_ASPECT_COLOR_BIT);
	VkSampler texture_sampler = fetch_sampler(sampler_type);

	return GraphicsEngineTexture(texture_image, texture_image_memory, texture_image_view, texture_sampler, dim);
}

template<typename GraphicsEngineT>
glm::uvec3 GraphicsEngineTextureManager<GraphicsEngineT>::create_texture_image(const std::string_view texture_path,
                                                                         VkImage& texture_image,
                                                                         VkDeviceMemory& texture_image_memory)
{
	int width, height, channels;
	std::unique_ptr<stbi_uc, std::function<void(stbi_uc*)>> pixels( 
		stbi_load(texture_path.data(), &width, &height, &channels, STBI_rgb_alpha),
		// custom unique_ptr destructor
		[](stbi_uc* ptr) { stbi_image_free(ptr); });

	// VkDeviceSize size = width * height * channels; // for some reason channels = 3?
	VkDeviceSize size = width * height * 4;
	if (!pixels.get())
	{
		throw std::runtime_error(fmt::format("failed to load texture image! {}", texture_path));
	}

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
	get_graphics_engine().transition_image_layout(
		texture_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	get_rsrc_mgr().stage_data_to_image(
		texture_image, 
		static_cast<uint32_t>(width), 
		static_cast<uint32_t>(height),
		static_cast<size_t>(size),
		[&pixels, &size](std::byte* destination)
		{
			memcpy(destination, pixels.get(), static_cast<size_t>(size));
		});

	// transition one more time for shader access
	get_graphics_engine().transition_image_layout(
		texture_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	LOG_INFO(Utility::get().get_logger(), 
			 "GraphicsEngineTextureManager::create_texture_image: created texture from:={}", 
			 texture_path);

	return glm::uvec3(width, height, channels);
}

template<typename GraphicsEngineT>
VkSampler GraphicsEngineTextureManager<GraphicsEngineT>::create_texture_sampler(ETextureSamplerType sampler_type)
{
	VkSamplerAddressMode address_mode;
	switch (sampler_type)
	{
	case ETextureSamplerType::ADDR_MODE_REPEAT:
		address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		break;
	case ETextureSamplerType::ADDR_MODE_CLAMP_TO_EDGE:
		address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		break;
	default:
		throw std::invalid_argument("unsupported texture sampler type!");
	}
	
	VkSamplerCreateInfo sampler_info{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
	sampler_info.magFilter = VK_FILTER_LINEAR; // how to interpolate texels that are magnified, solves oversampling
	sampler_info.minFilter = VK_FILTER_LINEAR; // how to interpolate texels that are minimised, solves undersampling
	// U,V,W is convention for texture space dimensions
	sampler_info.addressModeU = address_mode;
	sampler_info.addressModeV = address_mode;
	sampler_info.addressModeW = address_mode;
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

	VkSampler texture_sampler;
	if (vkCreateSampler(get_logical_device(), &sampler_info, nullptr, &texture_sampler) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create texture sampler!");
	}	

	return texture_sampler;
}

template<typename GraphicsEngineT>
void GraphicsEngineTextureManager<GraphicsEngineT>::copy_buffer_to_image(
	VkBuffer buffer,
	VkImage image,
	uint32_t width,
	uint32_t height)
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
