#pragma once

#include "graphics_engine_base_module.hpp"


class GraphicsEngineDepthBuffer : public GraphicsEngineBaseModule
{
public:
	GraphicsEngineDepthBuffer(GraphicsEngine& engine);
	~GraphicsEngineDepthBuffer();

	VkImageView& get_image_view() { return view; }

private:
	VkFormat find_supported_format(std::vector<VkFormat> candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
	VkFormat findDepthFormat();
	bool hasStencilComponent(VkFormat format);

	VkImage image;
	VkImageView view;
	VkDeviceMemory memory;
};