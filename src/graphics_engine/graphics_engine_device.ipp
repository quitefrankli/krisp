#pragma once

#include "graphics_engine.hpp"
#include "queues.hpp"
#include "utility_functions.hpp"
#include "utility.hpp"

#include <quill/Quill.h>

#include <iostream>
#include <set>
#include <string_view>


template<typename GraphicsEngineT>
GraphicsEngineDevice<GraphicsEngineT>::GraphicsEngineDevice(GraphicsEngineT& engine) :
	GraphicsEngineBaseModule<GraphicsEngineT>(engine)
{
	pick_physical_device();
	print_physical_device_settings();
	create_logical_device();
}

template<typename GraphicsEngineT>
GraphicsEngineDevice<GraphicsEngineT>::~GraphicsEngineDevice()
{
	vkDestroyDevice(get_logical_device(), nullptr);
	// note that physical device does not need to be destroyed
}

template<typename GraphicsEngineT>
void GraphicsEngineDevice<GraphicsEngineT>::pick_physical_device()
{
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(get_instance(), &deviceCount, nullptr);
	if (deviceCount == 0) {
		throw std::runtime_error("failed to find GPUs with Vulkan support!");
	}
	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(get_instance(), &deviceCount, devices.data());

	auto isDeviceSuitable = [this](VkPhysicalDevice device)
	{
		// basic properties
		VkPhysicalDeviceProperties deviceProperties;
		// more advanced properties
		VkPhysicalDeviceFeatures deviceFeatures;
		vkGetPhysicalDeviceProperties(device, &deviceProperties);
		vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
		
		if (!check_device_extension_support(device))
		{
			return false;
		}

		if (!get_graphics_engine().findQueueFamilies(device).isComplete())
		{
			return false;
		}

		SwapChainSupportDetails swap_chain_support = GraphicsEngineSwapChain<GraphicsEngineT>::query_swap_chain_support(device, get_graphics_engine().get_window_surface());
		if (swap_chain_support.formats.empty() || swap_chain_support.presentModes.empty())
		{
			return false;
		}

		if (!deviceFeatures.samplerAnisotropy)
		{
			return false;
		}

		return true;
	};

	physicalDevice = VK_NULL_HANDLE;
	for (const auto& device : devices) {
		if (isDeviceSuitable(device)) {
			physicalDevice = device;
			break;
		}
	}

	if (physicalDevice == VK_NULL_HANDLE) {
		throw std::runtime_error("failed to find a suitable GPU!");
	}
}

template<typename GraphicsEngineT>
bool GraphicsEngineDevice<GraphicsEngineT>::check_device_extension_support(VkPhysicalDevice device)
{
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

	std::set<std::string_view> required_extensions(required_device_extensions.begin(), required_device_extensions.end());

	for (const auto& extension : availableExtensions)
	{
		LOG_INFO(Utility::get().get_logger(), "GraphicsEngineDevice: found physical device extension: {}", extension.extensionName);
		required_extensions.erase(std::string_view{extension.extensionName});
	}

	return required_extensions.empty();
}

template<typename GraphicsEngineT>
void GraphicsEngineDevice<GraphicsEngineT>::create_logical_device()
{
	QueueFamilyIndices indices = get_graphics_engine().findQueueFamilies(physicalDevice);

	std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
	std::vector<uint32_t> unique_queue_families{
		indices.graphicsFamily.value(),
		indices.presentFamily.value()
	};
	// vulkan allows for some queues to have higher priority than others
	const float queue_priority = 1.0f;
	for (auto queue_family : unique_queue_families)
	{
		VkDeviceQueueCreateInfo queue_create_info{};
		queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_create_info.queueFamilyIndex = queue_family;
		queue_create_info.queueCount = 1; // there really is not a need for more than 1 queue per physical device
		queue_create_info.pQueuePriorities = &queue_priority;
		queue_create_infos.push_back(queue_create_info);
	}

	VkDeviceCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	create_info.pQueueCreateInfos = queue_create_infos.data();
	create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
	create_info.enabledExtensionCount = static_cast<uint32_t>(required_device_extensions.size());
	create_info.ppEnabledExtensionNames = required_device_extensions.data();

	VkPhysicalDeviceAccelerationStructureFeaturesKHR acceleration_structure_features{};
	acceleration_structure_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;

	VkPhysicalDeviceRayTracingPipelineFeaturesKHR ray_tracing_pipeline_features{};
	ray_tracing_pipeline_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;

	VkPhysicalDeviceFeatures deviceFeatures{}; // special features, we request
	deviceFeatures.samplerAnisotropy = true;
	deviceFeatures.fillModeNonSolid = true;
	VkPhysicalDeviceFeatures2 device_features2{};
	device_features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	device_features2.features = deviceFeatures;

	// linked list of features
	device_features2.pNext = &acceleration_structure_features;
	acceleration_structure_features.pNext = &ray_tracing_pipeline_features;
	create_info.pNext = &device_features2;

	if (vkCreateDevice(physicalDevice, &create_info, nullptr, &logical_device) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create logical device!");
	}

	// retrieves the queue handles
	vkGetDeviceQueue(logical_device, indices.presentFamily.value(), 0, &get_graphics_engine().get_present_queue());
	vkGetDeviceQueue(logical_device, indices.graphicsFamily.value(), 0, &get_graphics_engine().get_graphics_queue());
}

template<typename GraphicsEngineT>
void GraphicsEngineDevice<GraphicsEngineT>::print_physical_device_settings()
{
	const auto& properties = get_physical_device_properties();

	std::cout << "Cached physical device properties:" << 
		"\n\tmaxBoundDescriptorSets: " << properties.properties.limits.maxBoundDescriptorSets <<
		"\n\tmaxMSAA_Samples: " << get_max_usable_msaa() << '\n';
}

template<typename GraphicsEngineT>
const VkPhysicalDeviceProperties2& GraphicsEngineDevice<GraphicsEngineT>::get_physical_device_properties()
{
	if (!physical_device_properties)
	{
		physical_device_properties = VkPhysicalDeviceProperties2{};
		physical_device_properties->sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		physical_device_properties->pNext = &ray_tracing_properties;
		vkGetPhysicalDeviceProperties2(get_physical_device(), &physical_device_properties.value());
	}

	return physical_device_properties.value();
}

template<typename GraphicsEngineT>
VkSampleCountFlagBits GraphicsEngineDevice<GraphicsEngineT>::get_max_usable_msaa()
{
	const auto& properties = get_physical_device_properties();

    VkSampleCountFlags counts = 
		properties.properties.limits.framebufferColorSampleCounts & 
		properties.properties.limits.framebufferDepthSampleCounts;
	if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
	if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
	if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
	if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
	if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
	if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

	return VK_SAMPLE_COUNT_1_BIT;
}
