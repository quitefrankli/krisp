#pragma once

#include "graphics_engine.hpp"
#include "queues.hpp"
#include "utility_functions.hpp"

#include <iostream>
#include <set>


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
		
		if (!this->check_device_extension_support(device, device_extensions))
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

		return deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
			deviceFeatures.geometryShader;
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
	} else {
		std::cout << "found a suitable GPU!\n";
	}
}

template<typename GraphicsEngineT>
bool GraphicsEngineDevice<GraphicsEngineT>::check_device_extension_support(VkPhysicalDevice device, std::vector<std::string> device_extensions)
{
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

	std::set<std::string> required_extensions(device_extensions.begin(), device_extensions.end());

	for (const auto& extension : availableExtensions)
	{
		required_extensions.erase(std::string{extension.extensionName});
	}

	return required_extensions.empty();
}

template<typename GraphicsEngineT>
void GraphicsEngineDevice<GraphicsEngineT>::create_logical_device()
{
	QueueFamilyIndices indices = get_graphics_engine().findQueueFamilies(physicalDevice);

	std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
	std::vector<uint32_t> uniqueue_queue_families{
		indices.graphicsFamily.value(),
		indices.presentFamily.value()
	};
	// vulkan allows for some queues to have higher priority than others
	const float queue_priority = 1.0f;
	for (auto queue_family : uniqueue_queue_families)
	{
		VkDeviceQueueCreateInfo queue_create_info{};
		queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_create_info.queueFamilyIndex = queue_family;
		queue_create_info.queueCount = 1; // there really is not a need for more than 1 queue per physical device
		queue_create_info.pQueuePriorities = &queue_priority;
		queue_create_infos.push_back(queue_create_info);
	}
	VkPhysicalDeviceFeatures deviceFeatures{}; // special features, we request
	deviceFeatures.samplerAnisotropy = true;
	deviceFeatures.fillModeNonSolid = true;
	
	VkDeviceCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	create_info.pQueueCreateInfos = queue_create_infos.data();
	create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
	create_info.pEnabledFeatures = &deviceFeatures;
	create_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());
	c_style_str_array c_style_device_extensions(device_extensions);
	create_info.ppEnabledExtensionNames = c_style_device_extensions.data();
	auto validation_layers = GraphicsEngineValidationLayer<GraphicsEngineT>::get_layers();
	create_info.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
	create_info.ppEnabledLayerNames = validation_layers.data();

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
	VkPhysicalDeviceProperties properties;
	vkGetPhysicalDeviceProperties(physicalDevice, &properties);

	std::cout << "Cached physical device properties:" << 
		"\n\tmaxBoundDescriptorSets: " << properties.limits.maxBoundDescriptorSets <<
		"\n\tmaxMSAA_Samples: " << get_max_usable_msaa() << '\n';
}

template<typename GraphicsEngineT>
const VkPhysicalDeviceProperties& GraphicsEngineDevice<GraphicsEngineT>::get_physical_device_properties()
{
	if (!bPhysicalDevicePropertiesCached)
	{
		vkGetPhysicalDeviceProperties(get_physical_device(), &physical_device_properties);			
		bPhysicalDevicePropertiesCached = true;
	}

	return physical_device_properties;
}

template<typename GraphicsEngineT>
VkSampleCountFlagBits GraphicsEngineDevice<GraphicsEngineT>::get_max_usable_msaa()
{
	auto properties = get_physical_device_properties();

    VkSampleCountFlags counts = properties.limits.framebufferColorSampleCounts & properties.limits.framebufferDepthSampleCounts;
	if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
	if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
	if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
	if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
	if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
	if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

	return VK_SAMPLE_COUNT_1_BIT;
}
