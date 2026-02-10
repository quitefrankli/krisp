#pragma once

#include "graphics_engine.hpp"
#include "queues.hpp"
#include "utility.hpp"

#include <quill/LogMacros.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ranges.h>

#include <iostream>
#include <set>
#include <string_view>
#include "graphics_engine_device.hpp"


GraphicsEngineDevice::GraphicsEngineDevice(GraphicsEngine& engine) :
	GraphicsEngineBaseModule(engine)
{
	pick_physical_device();
	print_physical_device_settings();
	create_logical_device();
}

GraphicsEngineDevice::~GraphicsEngineDevice()
{
	vkDestroyDevice(get_logical_device(), nullptr);
	// note that physical device does not need to be destroyed
}

void GraphicsEngineDevice::pick_physical_device()
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

		LOG_INFO(Utility::get_logger(), "GraphicsEngineDevice: checking device:={}", deviceProperties.deviceName);

		if (!check_device_extension_support(device))
		{
			return false;
		}

		if (!get_graphics_engine().findQueueFamilies(device).isComplete())
		{
			return false;
		}

		SwapChainSupportDetails swap_chain_support = GraphicsEngineSwapChain::query_swap_chain_support(device, get_graphics_engine().get_window_surface());
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

bool GraphicsEngineDevice::check_device_extension_support(VkPhysicalDevice device)
{
	const auto available_extensions = [&]()
	{
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

		std::vector<std::string> retval;
		std::transform(availableExtensions.begin(), availableExtensions.end(), std::back_inserter(retval), [](const auto& extension)
		{
			return extension.extensionName;
		});

		return retval;
	}();

	auto required_extensions_set = [&]()
	{
		const auto required_device_extensions = get_required_extensions();
		std::set<std::string_view> required_extensions(required_device_extensions.begin(), required_device_extensions.end());
		return required_extensions;
	}();

	std::for_each(available_extensions.begin(), available_extensions.end(), [&required_extensions_set](const auto& extension)
	{
		required_extensions_set.erase(extension.data());
	});

	LOG_INFO(Utility::get_logger(), 
			 "GraphicsEngineDevice: found physical device extensions: {}", 
			 fmt::format("{}", fmt::join(available_extensions, ", ")));
	
	return required_extensions_set.empty();
}

void GraphicsEngineDevice::create_logical_device()
{
	QueueFamilyIndices indices = get_graphics_engine().findQueueFamilies(physicalDevice);

	std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
	std::vector<uint32_t> unique_queue_families = [&indices]()
	{
		const std::set<uint32_t> unique_families{
			indices.graphicsFamily.value(),
			indices.presentFamily.value()
		};
		return std::vector<uint32_t>(unique_families.begin(), unique_families.end());
	}();

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

	VkDeviceCreateInfo create_info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
	create_info.pQueueCreateInfos = queue_create_infos.data();
	create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
	const auto required_device_extensions = get_required_extensions();
	create_info.enabledExtensionCount = static_cast<uint32_t>(required_device_extensions.size());
	create_info.ppEnabledExtensionNames = required_device_extensions.data();
	create_info.pNext = get_required_features();

	if (vkCreateDevice(physicalDevice, &create_info, nullptr, &logical_device) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create logical device!");
	}

	// retrieves the queue handles
	vkGetDeviceQueue(logical_device, indices.presentFamily.value(), 0, &get_graphics_engine().get_present_queue());
	vkGetDeviceQueue(logical_device, indices.graphicsFamily.value(), 0, &get_graphics_engine().get_graphics_queue());
}

void GraphicsEngineDevice::print_physical_device_settings()
{
	const auto& properties = get_physical_device_properties();

	LOG_INFO(Utility::get_logger(),
			 "Physical Device Properties: maxBoundDescriptorSets: {}, maxMSAA_Samples: {}, "
			 "maxRayRecursionDepth: {}, minStorageBufferOffsetAlignment: {}",
			 properties.properties.limits.maxBoundDescriptorSets,
			 int(get_max_usable_msaa()),
			 ray_tracing_properties.maxRayRecursionDepth,
			 size_t(properties.properties.limits.minStorageBufferOffsetAlignment));
}

const VkPhysicalDeviceProperties2& GraphicsEngineDevice::get_physical_device_properties()
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

VkDeviceAddress GraphicsEngineDevice::get_buffer_device_address(VkBuffer buffer)
{
	VkBufferDeviceAddressInfo info{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
	info.buffer = buffer;
	return vkGetBufferDeviceAddress(get_logical_device(), &info);
}

std::vector<const char*> GraphicsEngineDevice::get_required_extensions()
{
	std::vector<const char*> required_device_extensions;

	required_device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
	required_device_extensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
	required_device_extensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
	required_device_extensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);

#ifdef _DEBUG
	// TODO: enable if want shader printf
	// required_device_extensions.push_back(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME);
#endif

	return required_device_extensions;
}

VkPhysicalDeviceFeatures2* GraphicsEngineDevice::get_required_features()
{
	static VkPhysicalDeviceAccelerationStructureFeaturesKHR acceleration_structure_features{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
	acceleration_structure_features.accelerationStructure = true;

	static VkPhysicalDeviceRayTracingPipelineFeaturesKHR ray_tracing_pipeline_features{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR};
	ray_tracing_pipeline_features.rayTracingPipeline = true;

	// special features, we request
	static VkPhysicalDeviceFeatures2 device_features2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
	device_features2.features.samplerAnisotropy = true;
	device_features2.features.fillModeNonSolid = true;

	static VkPhysicalDeviceVulkan12Features device_features12{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
	device_features12.bufferDeviceAddress = true;

	// link up the structs to create a chain of features
	device_features2.pNext = &device_features12;
	device_features12.pNext = &acceleration_structure_features;
	acceleration_structure_features.pNext = &ray_tracing_pipeline_features;

	return &device_features2;
}

VkSampleCountFlagBits GraphicsEngineDevice::get_max_usable_msaa()
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
