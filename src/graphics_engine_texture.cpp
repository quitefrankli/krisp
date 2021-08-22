#include "graphics_engine_texture.hpp"

#include "graphics_engine.hpp"

#include "vulkan/vulkan.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

const std::string TEXTURE_PATH = "../resources/textures/";

void GraphicsEngineTexture::init(GraphicsEngine* graphics_engiine)
{
	this->graphics_engine = graphics_engiine;
	create_texture_image();
}

void GraphicsEngineTexture::create_image(uint32_t width, 
										 uint32_t height, 
										 VkFormat format, 
										 VkImageTiling tiling,
										 VkImageUsageFlags usage,
										 VkMemoryPropertyFlags properties,
										 VkImage& image,
										 VkDeviceMemory& image_memory)
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
	image_info.tiling = tiling; // types include:
												 // LINEAR - texels are laid out in row major order
												 // OPTIMAL - texels are laid out in an implementation defined order
	image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // UNDEFINED = not usable by GPU and first transition will discard texels
														  // PREINITIALIZED = not usable by GPU and first transition will preserve texels
	image_info.usage = usage; 
	image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // will only be used by one queue family
	image_info.samples = VK_SAMPLE_COUNT_1_BIT; // for multisampling
	image_info.flags = 0;

	if (vkCreateImage(graphics_engine->get_logical_device(), &image_info, nullptr, &image) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create image!");
	}

	//
	// allocate memory for an image
	//

	VkMemoryRequirements mem_req;
	vkGetImageMemoryRequirements(graphics_engine->get_logical_device(), image, &mem_req);
	VkMemoryAllocateInfo alloc_info{};
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = mem_req.size;
	alloc_info.memoryTypeIndex = graphics_engine->find_memory_type(mem_req.memoryTypeBits, properties);

	if (vkAllocateMemory(graphics_engine->get_logical_device(), &alloc_info, nullptr, &image_memory) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate image memory!");
	}

	vkBindImageMemory(graphics_engine->get_logical_device(), image, image_memory, 0);
}

// load image and upload it into a vulkan image object
void GraphicsEngineTexture::create_texture_image()
{
	int width, height, channels;
	const std::string texture_path = TEXTURE_PATH + "texture.jpg";
	std::unique_ptr<stbi_uc, std::function<void(stbi_uc*)>> pixels(
		stbi_load(texture_path.c_str(), &width, &height, &channels, STBI_rgb_alpha), 
		[](stbi_uc* ptr) { stbi_image_free(ptr); });

	VkDeviceSize size = width * height * channels;
	if (!pixels.get())
	{
		throw std::runtime_error("failed to load texture image!");
	}

	VkBuffer staging_buffer;
	VkDeviceMemory staging_buffer_memory;

	graphics_engine->create_buffer(size, 
				  VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
				  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				  staging_buffer,
				  staging_buffer_memory);

	void* data;
	vkMapMemory(graphics_engine->get_logical_device(), staging_buffer_memory, 0, size, 0, &data);
	memcpy(data, pixels.get(), static_cast<size_t>(size));
	vkUnmapMemory(graphics_engine->get_logical_device(), staging_buffer_memory);

	create_image(width, 
				 height, 
				 VK_FORMAT_R8G8B8A8_SRGB, // we may want to reconsider SRGB
				 VK_IMAGE_TILING_OPTIMAL,
				 VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, // we want to use it as dest and be able to access it from shader to colour the mesh
				 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				 texture_image,
				 texture_image_memory);
}