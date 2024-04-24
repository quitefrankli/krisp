#pragma once

#include "graphics_engine_base_module.hpp"

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_beta.h>

#include <optional>


class GraphicsEngineDevice : public GraphicsEngineBaseModule
{
public:
	GraphicsEngineDevice(GraphicsEngine& engine);
	~GraphicsEngineDevice();
	
	VkDevice& get_logical_device() override { return logical_device; }
	VkPhysicalDevice& get_physical_device() override { return physicalDevice; }

	void print_physical_device_settings();

	VkSampleCountFlagBits get_max_usable_msaa();

	const VkPhysicalDeviceProperties2& get_physical_device_properties();
	VkDeviceAddress get_buffer_device_address(VkBuffer buffer);	

	VkPhysicalDeviceRayTracingPipelinePropertiesKHR get_ray_tracing_properties() const 
	{ 
		return ray_tracing_properties; 
	}

private:
	
	VkPhysicalDevice physicalDevice;
	VkDevice logical_device;

	static std::vector<const char*> get_required_extensions();

	// returns vulkan style linked list of features
	static VkPhysicalDeviceFeatures2* get_required_features();

	void pick_physical_device();
	void create_logical_device();
	bool check_device_extension_support(VkPhysicalDevice device);

	std::optional<VkPhysicalDeviceProperties2> physical_device_properties;
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR ray_tracing_properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};
};