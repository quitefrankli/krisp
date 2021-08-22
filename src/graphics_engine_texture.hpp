#include <vulkan/vulkan.hpp>

class GraphicsEngine;

class GraphicsEngineTexture
{
	// while shaders can access pixel values in the buffer,
	// it's better to use Vk Image Objects as there are some optimisations due
	// to the 2D nature of textures
	VkImage texture_image;
	VkDeviceMemory texture_image_memory;

	void create_texture_image();

	GraphicsEngine* graphics_engine = nullptr;

public:
	void init(GraphicsEngine*);

	void create_image(uint32_t width, 
					  uint32_t height, 
					  VkFormat format, 
					  VkImageTiling tiling,
					  VkImageUsageFlags usage,
					  VkMemoryPropertyFlags properties,
					  VkImage& image,
					  VkDeviceMemory& image_memory);
};