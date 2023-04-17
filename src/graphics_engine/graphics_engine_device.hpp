#pragma once

#include "graphics_engine_base_module.hpp"

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_beta.h>

#include <optional>


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

	const VkPhysicalDeviceProperties2& get_physical_device_properties();
	VkDeviceAddress get_buffer_device_address(VkBuffer buffer);	

	VkPhysicalDeviceRayTracingPipelinePropertiesKHR get_ray_tracing_properties() const 
	{ 
		return ray_tracing_properties; 
	}

private:
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_graphics_engine;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_physical_device;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_instance;
	using GraphicsEngineBaseModule<GraphicsEngineT>::create_buffer;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_num_swapchain_frames;
	
	VkPhysicalDevice physicalDevice;
	VkDevice logical_device;

#ifdef __APPLE__
	static constexpr std::array<const char*, 2> required_device_extensions{{ 
		(const char*)(VK_KHR_SWAPCHAIN_EXTENSION_NAME), 
		(const char*)(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME)
	}};
#else
	static constexpr std::array<const char*, 4> required_device_extensions = { 
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
		VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
		VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME
	};
#endif
	void pick_physical_device();
	void create_logical_device();
	bool check_device_extension_support(VkPhysicalDevice device);

	std::optional<VkPhysicalDeviceProperties2> physical_device_properties;
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR ray_tracing_properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};
};