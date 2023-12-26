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
	for (auto& [texture_id, texture_unit] : texture_units)
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
	const MaterialTexture& material_texture,
	ETextureSamplerType sampler_type)
{
	if (material_texture.texture_id >= CUBE_MAP_OFFSET)
	{
		throw std::invalid_argument("GraphicsEngineTextureManager: normal textures dont support id>=CUBE_MAP_OFFSET");
	}

	auto it = texture_units.find(material_texture.texture_id);
	if (it != texture_units.end())
	{
		return it->second;
	}

	return texture_units.emplace(material_texture.texture_id, create_texture(material_texture, sampler_type)).first->second;
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
GraphicsEngineTexture& GraphicsEngineTextureManager<GraphicsEngineT>::fetch_cubemap_texture(
	const std::vector<MaterialTexture>& material_textures)
{
	const uint32_t cubemap_texture_id = CUBE_MAP_OFFSET + material_textures[0].texture_id;
	if (texture_units.find(cubemap_texture_id) != texture_units.end())
	{
		return texture_units.at(cubemap_texture_id);
	}

	const uint32_t width = material_textures[0].width;
	const uint32_t height = material_textures[0].height;
	const uint32_t channels = material_textures[0].channels;
	// only RGBA is currently supported
	assert(channels == 4);
	if (!std::all_of(
		material_textures.begin(), 
		material_textures.end(), 
		[&width, &height](const MaterialTexture& material_texture) 
		{ 
			return width == material_texture.width && height == material_texture.height; 
		}))
	{
		throw std::runtime_error("GraphicsEngineTextureManager::create_volume_texture: supplied material textures are not all the same size!");
	}

	VkImage texture_image;
	VkDeviceMemory texture_image_memory;
	create_cubemap_texture_image(material_textures, texture_image, texture_image_memory);

	VkImageView texture_image_view = get_graphics_engine().create_image_view(
		texture_image, 
		VK_FORMAT_R8G8B8A8_SRGB, // assume textures are gamma corrected
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_VIEW_TYPE_CUBE,
		material_textures.size());
	VkSampler texture_sampler = fetch_sampler(ETextureSamplerType::ADDR_MODE_REPEAT);

	GraphicsEngineTexture texture_object(
		texture_image, 
		texture_image_memory, 
		texture_image_view, 
		texture_sampler, 
		glm::uvec3(width, height, channels));
	return texture_units.emplace(cubemap_texture_id, std::move(texture_object)).first->second;
}

template<typename GraphicsEngineT>
GraphicsEngineTexture GraphicsEngineTextureManager<GraphicsEngineT>::create_texture(
	const MaterialTexture& material_texture,
	ETextureSamplerType sampler_type)
{
	VkImage texture_image;
	VkDeviceMemory texture_image_memory;
	const auto dim = create_texture_image(material_texture, texture_image, texture_image_memory);
	VkImageView texture_image_view = get_graphics_engine().create_image_view(
		texture_image, 
		VK_FORMAT_R8G8B8A8_SRGB, // assume textures are gamma corrected
		VK_IMAGE_ASPECT_COLOR_BIT);
	VkSampler texture_sampler = fetch_sampler(sampler_type);

	return GraphicsEngineTexture(texture_image, texture_image_memory, texture_image_view, texture_sampler, dim);
}

template<typename GraphicsEngineT>
glm::uvec3 GraphicsEngineTextureManager<GraphicsEngineT>::create_texture_image(
	const MaterialTexture& material_texture,
	VkImage& texture_image,
	VkDeviceMemory& texture_image_memory)
{
	assert(material_texture.channels == 4); // only RGBA is currently supported atm
	VkDeviceSize size = material_texture.width * material_texture.height * material_texture.channels;

	if (size == 0)
	{
		throw std::runtime_error("GraphicsEngineTextureManager::create_texture_image: supplied material texture is invalid, size=0!");
	}

	get_graphics_engine().create_image(
		material_texture.width, 
		material_texture.height, 
		VK_FORMAT_R8G8B8A8_SRGB, // we may want to reconsider SRGB, for other maps such as normal and specular maps they should be linear
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
		material_texture.width, 
		material_texture.height,
		static_cast<size_t>(size),
		[&material_texture, &size](std::byte* destination)
		{
			std::memcpy(destination, material_texture.data, static_cast<size_t>(size));
		});

	// transition one more time for shader access
	get_graphics_engine().transition_image_layout(
		texture_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	return glm::uvec3(material_texture.width, material_texture.height, material_texture.channels);
}

template<typename GraphicsEngineT>
void GraphicsEngineTextureManager<GraphicsEngineT>::create_cubemap_texture_image(
	const std::vector<MaterialTexture>& material_textures,
	VkImage& texture_image,
	VkDeviceMemory& texture_image_memory)
{
	const uint32_t width = material_textures[0].width;
	const uint32_t height = material_textures[0].height;
	const uint32_t channels = material_textures[0].channels;

	VkDeviceSize layer_size = width * height * channels;
	VkDeviceSize image_size = layer_size * material_textures.size();
	if (layer_size == 0)
	{
		throw std::runtime_error("GraphicsEngineTextureManager::create_volume_texture: supplied material texture is invalid, size=0!");
	}

	get_graphics_engine().create_image(
		width, 
		height, 
		VK_FORMAT_R8G8B8A8_SRGB, // we may want to reconsider SRGB, for other maps such as normal and specular maps they should be linear
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, // we want to use it as dest and be able to access it from shader to colour the mesh
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		texture_image,
		texture_image_memory,
		VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT,
		material_textures.size(),
		VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);

	// copy the staging buffer to the texture image,
	// undefined image layout works because we don't care about the contents before performing copy
	get_graphics_engine().transition_image_layout(
		texture_image, 
		VK_IMAGE_LAYOUT_UNDEFINED, 
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		nullptr,
		material_textures.size());

	get_rsrc_mgr().stage_data_to_image(
		texture_image, 
		width, 
		height,
		static_cast<size_t>(image_size),
		[&material_textures, &layer_size](std::byte* destination)
		{
			for (auto i = 0; i < material_textures.size(); i++)
			{
				std::memcpy(destination+(layer_size*i), material_textures[i].data, static_cast<size_t>(layer_size));
			}
		},
		material_textures.size());

	// transition one more time for shader access
	get_graphics_engine().transition_image_layout(
		texture_image, 
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		nullptr,
		material_textures.size());
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