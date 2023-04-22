#pragma once

#include <vulkan/vulkan.hpp>
#include <glm/ext/vector_uint3.hpp>


template<typename GraphicsEngineT>
class GraphicsEngineTextureManager;

enum class ETextureSamplerType
{
	ADDR_MODE_REPEAT,
	ADDR_MODE_CLAMP_TO_EDGE
};

struct GraphicsEngineTexture
{
public:
	GraphicsEngineTexture(
		VkImage texture_image,
		VkDeviceMemory texture_image_memory,
		VkImageView texture_image_view,
		VkSampler texture_sampler,
		const glm::uvec3& dimensions);
	GraphicsEngineTexture(GraphicsEngineTexture&& other) noexcept;
	~GraphicsEngineTexture();

	void destroy(VkDevice device);

	VkImageView get_texture_image_view() const { return texture_image_view; }
	VkSampler get_texture_sampler() const { return texture_sampler; }

	uint32_t get_width() const { return dimensions.x; }
	uint32_t get_height() const { return dimensions.y; }
	uint32_t get_depth() const { return dimensions.z; }

private:
	// while shaders can access pixel values in the buffer,
	// it's better to use Vk Image Objects as there are some optimisations due
	// to the 2D nature of textures
	VkImage texture_image = nullptr;
	
	/*
		while it's possible for shaders to read texels directly off the image, it's better for the shaders to access the texels
		through a sampler which allows for filtering and transformations. Examples include:
		* Bilinear filtering
		* Anisotropic filtering
		* Repeat/clamp to edge/clamp to border
	*/
	VkSampler texture_sampler = nullptr;
	VkImageView texture_image_view = nullptr;
	VkDeviceMemory texture_image_memory = nullptr;
	const glm::uvec3 dimensions;
};