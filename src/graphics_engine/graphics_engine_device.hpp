#pragma once

#include "graphics_engine_base_module.hpp"

#include <vulkan/vulkan.hpp>


class GraphicsEngine;

class GraphicsEngineDevice : public GraphicsEngineBaseModule
{
public:
	GraphicsEngineDevice(GraphicsEngine& engine);
	~GraphicsEngineDevice();
	
	VkDevice& get_logical_device() override { return logical_device; }
	VkPhysicalDevice& get_physical_device() override { return physicalDevice; }
private:
	VkPhysicalDevice physicalDevice;
	VkDevice logical_device;

	const std::vector<std::string> device_extensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };

	void pick_physical_device();
	void create_logical_device();
	bool check_device_extension_support(VkPhysicalDevice device, std::vector<std::string> device_extensions);
};