#pragma once

#include "graphics_engine_base_module.hpp"


template<typename GraphicsEngineT>
class GraphicsEngineDepthBuffer : public GraphicsEngineBaseModule<GraphicsEngineT>
{
public:
	GraphicsEngineDepthBuffer(GraphicsEngineT& engine);
	~GraphicsEngineDepthBuffer();

	VkImageView& get_image_view() { return view; }

	static VkFormat find_supported_format(VkPhysicalDevice device, std::vector<VkFormat> candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
	static VkFormat findDepthFormat(VkPhysicalDevice device);
	
private:
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_graphics_engine;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_logical_device;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_physical_device;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_instance;
	using GraphicsEngineBaseModule<GraphicsEngineT>::create_buffer;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_num_swapchain_frames;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_render_pass;

	bool hasStencilComponent(VkFormat format);

	VkImage image;
	VkImageView view;
	VkDeviceMemory memory;
};