#pragma once

#include "graphics_engine_base_module.hpp"

#include <vulkan/vulkan.hpp>


template<typename GraphicsEngineT>
class GraphicsEngineDevice : public GraphicsEngineBaseModule<GraphicsEngineT>
{
public:
	GraphicsEngineDevice(GraphicsEngineT& engine);
	~GraphicsEngineDevice();
	
	VkDevice& get_logical_device() override { return logical_device; }
	VkPhysicalDevice& get_physical_device() override { return physicalDevice; }

	void print_physical_device_settings();

	VkSampleCountFlagBits get_max_usable_msaa();

	const VkPhysicalDeviceProperties& get_physical_device_properties();

private:
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_graphics_engine;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_logical_device;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_physical_device;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_instance;
	using GraphicsEngineBaseModule<GraphicsEngineT>::create_buffer;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_num_swapchain_frames;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_render_pass;
	
	bool bPhysicalDevicePropertiesCached = false;

	VkPhysicalDevice physicalDevice;
	VkDevice logical_device;

	const std::vector<std::string> device_extensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };

	void pick_physical_device();
	void create_logical_device();
	bool check_device_extension_support(VkPhysicalDevice device, std::vector<std::string> device_extensions);

	VkPhysicalDeviceProperties physical_device_properties;
};