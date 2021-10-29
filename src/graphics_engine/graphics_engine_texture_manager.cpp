#include "graphics_engine_texture_manager.hpp"

#include "graphics_engine.hpp"

#include <iostream>


GraphicsEngineTextureManager::GraphicsEngineTextureManager(GraphicsEngine &engine) : GraphicsEngineBaseModule(engine)
{
}

GraphicsEngineTexture &GraphicsEngineTextureManager::create_new_unit(std::string texture_path)
{
	auto it = texture_units.find(texture_path);
	if (it != texture_units.end())
	{
		return it->second;
	}

	// note there is a bug that can cause emplace to still construct an object which is why
	// the above logic is required
	it = texture_units.emplace(std::piecewise_construct, 
									std::forward_as_tuple(texture_path), 
									std::forward_as_tuple(*this, texture_path)).first;
	return it->second;
}

void GraphicsEngineTextureManager::create_image(uint32_t width,
												uint32_t height,
												VkFormat format,
												VkImageTiling tiling,
												VkImageUsageFlags usage,
												VkMemoryPropertyFlags properties,
												VkImage &image,
												VkDeviceMemory &image_memory)
{
	VkImageCreateInfo image_info{};
	image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_info.imageType = VK_IMAGE_TYPE_2D; // 1D for array of data or gradient, 3D for voxels
	image_info.extent.width = static_cast<uint32_t>(width);
	image_info.extent.height = static_cast<uint32_t>(height);
	image_info.extent.depth = 1;
	image_info.mipLevels = 1; // mip mapping
	image_info.arrayLayers = 1;
	image_info.format = format;
	image_info.tiling = tiling;							  // types include:
														  // LINEAR - texels are laid out in row major order
														  // OPTIMAL - texels are laid out in an implementation defined order
	image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // UNDEFINED = not usable by GPU and first transition will discard texels
														  // PREINITIALIZED = not usable by GPU and first transition will preserve texels
	image_info.usage = usage;
	image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // will only be used by one queue family
	image_info.samples = VK_SAMPLE_COUNT_1_BIT;			// for multisampling
	image_info.flags = 0;

	if (vkCreateImage(get_logical_device(), &image_info, nullptr, &image) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create image!");
	}

	//
	// allocate memory for an image
	//

	VkMemoryRequirements mem_req;
	vkGetImageMemoryRequirements(get_logical_device(), image, &mem_req);
	VkMemoryAllocateInfo alloc_info{};
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = mem_req.size;
	alloc_info.memoryTypeIndex = get_graphics_engine().find_memory_type(mem_req.memoryTypeBits, properties);

	if (vkAllocateMemory(get_logical_device(), &alloc_info, nullptr, &image_memory) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate image memory!");
	}

	vkBindImageMemory(get_logical_device(), image, image_memory, 0);
}

VkImageView GraphicsEngineTextureManager::create_image_view(VkImage &image,
															VkFormat format,
															VkImageAspectFlags aspect_flags)
{
	VkImageViewCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	create_info.image = image;
	create_info.viewType = VK_IMAGE_VIEW_TYPE_2D; // specifies how the image data should be interpreted
												  // i.e. treat images as 1D, 2D, 3D textures and cube maps
	create_info.format = format;
	create_info.subresourceRange.aspectMask = aspect_flags; // describes image purpose and which part should be accessed
	create_info.subresourceRange.baseMipLevel = 0;
	create_info.subresourceRange.levelCount = 1;
	create_info.subresourceRange.baseArrayLayer = 0;
	create_info.subresourceRange.layerCount = 1;

	VkImageView image_view;
	if (vkCreateImageView(get_logical_device(), &create_info, nullptr, &image_view) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create image views!");
	}

	return image_view;
}