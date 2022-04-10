#pragma once

#include "graphics_engine_base_module.hpp"


class GraphicsEngineDepthBuffer : public GraphicsEngineBaseModule
{
public:
	GraphicsEngineDepthBuffer(GraphicsEngine& engine);
	~GraphicsEngineDepthBuffer();

	VkImageView& get_image_view() { return view; }

	static VkFormat find_supported_format(VkPhysicalDevice device, std::vector<VkFormat> candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
	static VkFormat findDepthFormat(VkPhysicalDevice device);
	
private:
	bool hasStencilComponent(VkFormat format);

	VkImage image;
	VkImageView view;
	VkDeviceMemory memory;
};