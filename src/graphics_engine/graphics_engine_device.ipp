#pragma once

#include "graphics_engine.hpp"
#include "point_lighting.hpp"
#include "queues.hpp"
#include "utility.hpp"

#include <quill/LogMacros.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ranges.h>

#include <iostream>
#include <set>
#include <string_view>
#include <cstring>
#include "graphics_engine_device.hpp"


GraphicsEngineDevice::GraphicsEngineDevice(GraphicsEngine& engine) :
	GraphicsEngineBaseModule(engine)
{
	pick_physical_device();
	print_physical_device_settings();
	create_logical_device();
	VmaAllocatorCreateInfo allocator_info{};
	allocator_info.instance = get_instance();
	allocator_info.physicalDevice = get_physical_device();
	allocator_info.device = get_logical_device();
	allocator_info.vulkanApiVersion = VK_API_VERSION_1_3;
	if (vmaCreateAllocator(&allocator_info, &allocator) != VK_SUCCESS)
	{
		vkDestroyDevice(get_logical_device(), nullptr);
		throw std::runtime_error("failed to create GPU memory allocator");
	}
}

GraphicsEngineDevice::~GraphicsEngineDevice()
{
	vmaDestroyAllocator(allocator);
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
		vkGetPhysicalDeviceProperties(device, &deviceProperties);
		VkPhysicalDeviceVulkan13Features vulkan13{
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
		VkPhysicalDeviceFeatures2 supported_features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
		supported_features.pNext = &vulkan13;
		vkGetPhysicalDeviceFeatures2(device, &supported_features);
		if (deviceProperties.apiVersion < VK_API_VERSION_1_3
			|| !vulkan13.dynamicRendering || !vulkan13.synchronization2)
		{
			LOG_WARNING(Utility::get_logger(),
				"GraphicsEngineDevice: {} lacks Vulkan 1.3 dynamic rendering or synchronization2",
				deviceProperties.deviceName);
			return false;
		}

		LOG_INFO(Utility::get_logger(), "GraphicsEngineDevice: checking device:={}", deviceProperties.deviceName);

		if (!check_device_extension_support(device))
		{
			return false;
		}
		const char* maintenance_extension = get_swapchain_maintenance_extension(device);
		if (maintenance_extension == nullptr)
		{
			LOG_WARNING(Utility::get_logger(),
				"GraphicsEngineDevice: {} lacks a supported KHR/EXT swapchain maintenance extension and surface maintenance dependency",
				deviceProperties.deviceName);
			return false;
		}
		if (!supports_required_features(device))
		{
			LOG_WARNING(Utility::get_logger(),
				"GraphicsEngineDevice: {} lacks the required swapchainMaintenance1 feature",
				deviceProperties.deviceName);
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

		if (!supported_features.features.samplerAnisotropy)
		{
			return false;
		}
		if (!supported_features.features.geometryShader)
		{
			return false;
		}
		if (!supported_features.features.imageCubeArray
			|| deviceProperties.limits.maxImageArrayLayers < 6 * SDS::MAX_POINT_LIGHTS
			|| deviceProperties.limits.maxFramebufferLayers < 6 * SDS::MAX_POINT_LIGHTS
			|| deviceProperties.limits.maxImageDimensionCube < POINT_SHADOW_RESOLUTION
			|| deviceProperties.limits.maxUniformBufferRange < sizeof(SDS::GlobalData))
		{
			LOG_WARNING(Utility::get_logger(),
				"GraphicsEngineDevice: {} lacks point-light cube-array capacity",
				deviceProperties.deviceName);
			return false;
		}
		VkImageFormatProperties shadow_properties{};
		VkFormatProperties shadow_format{};
		vkGetPhysicalDeviceFormatProperties(device, VK_FORMAT_D32_SFLOAT, &shadow_format);
		constexpr auto required_shadow_features = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
			| VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
		if ((shadow_format.optimalTilingFeatures & required_shadow_features) != required_shadow_features
			|| vkGetPhysicalDeviceImageFormatProperties(device, VK_FORMAT_D32_SFLOAT,
				VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, &shadow_properties) != VK_SUCCESS
			|| shadow_properties.maxArrayLayers < 6 * SDS::MAX_POINT_LIGHTS
			|| shadow_properties.maxExtent.width < POINT_SHADOW_RESOLUTION
			|| shadow_properties.maxExtent.height < POINT_SHADOW_RESOLUTION)
		{
			LOG_WARNING(Utility::get_logger(),
				"GraphicsEngineDevice: {} lacks D32 sampled shadow-array support",
				deviceProperties.deviceName);
			return false;
		}
		if (!supported_features.features.textureCompressionBC)
		{
			return false;
		}

		VkFormatProperties hdr_format_properties{};
		vkGetPhysicalDeviceFormatProperties(
			device, VK_FORMAT_R16G16B16A16_SFLOAT, &hdr_format_properties);
		constexpr VkFormatFeatureFlags required_hdr_features =
			VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
			VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT |
			VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
		if ((hdr_format_properties.optimalTilingFeatures & required_hdr_features) != required_hdr_features)
		{
			LOG_WARNING(Utility::get_logger(),
				"GraphicsEngineDevice: {} lacks RGBA16F color-attachment/sample support required for HDR",
				deviceProperties.deviceName);
			return false;
		}
		VkImageFormatProperties hdr_msaa_properties{};
		if (vkGetPhysicalDeviceImageFormatProperties(
				device,
				VK_FORMAT_R16G16B16A16_SFLOAT,
				VK_IMAGE_TYPE_2D,
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
				0,
				&hdr_msaa_properties) != VK_SUCCESS ||
			!(hdr_msaa_properties.sampleCounts &
			  static_cast<VkSampleCountFlagBits>(CSTS::MSAA_SAMPLE_COUNT)))
		{
			LOG_WARNING(Utility::get_logger(),
				"GraphicsEngineDevice: {} lacks the required RGBA16F MSAA sample count",
				deviceProperties.deviceName);
			return false;
		}
		VkImageFormatProperties hdr_resolve_properties{};
		if (vkGetPhysicalDeviceImageFormatProperties(
				device,
				VK_FORMAT_R16G16B16A16_SFLOAT,
				VK_IMAGE_TYPE_2D,
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				0,
				&hdr_resolve_properties) != VK_SUCCESS ||
			!(hdr_resolve_properties.sampleCounts & VK_SAMPLE_COUNT_1_BIT))
		{
			LOG_WARNING(Utility::get_logger(),
				"GraphicsEngineDevice: {} cannot create the sampled RGBA16F resolve image",
				deviceProperties.deviceName);
			return false;
		}

		return true;
	};

	physicalDevice = VK_NULL_HANDLE;
	for (const auto& device : devices) {
		if (isDeviceSuitable(device)) {
			physicalDevice = device;
			swapchain_maintenance_extension = get_swapchain_maintenance_extension(device);
			break;
		}
	}

	if (physicalDevice == VK_NULL_HANDLE) {
		throw std::runtime_error(
		"failed to find a suitable GPU (requires Vulkan 1.3 dynamic rendering, synchronization2, sampled RGBA16F color attachments, and enabled KHR/EXT swapchain maintenance1 support)");
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
		const auto required_device_extensions = get_required_extensions(nullptr);
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

const char* GraphicsEngineDevice::get_swapchain_maintenance_extension(VkPhysicalDevice device)
{
	uint32_t device_extension_count = 0;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &device_extension_count, nullptr);
	std::vector<VkExtensionProperties> device_extensions(device_extension_count);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &device_extension_count, device_extensions.data());
	const auto has_device_extension = [&device_extensions](const char* name) {
		return std::any_of(device_extensions.begin(), device_extensions.end(), [name](const auto& extension) {
			return std::strcmp(extension.extensionName, name) == 0;
		});
	};

	uint32_t instance_extension_count = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &instance_extension_count, nullptr);
	std::vector<VkExtensionProperties> instance_extensions(instance_extension_count);
	vkEnumerateInstanceExtensionProperties(nullptr, &instance_extension_count, instance_extensions.data());
	const auto has_instance_extension = [&instance_extensions](const char* name) {
		return std::any_of(instance_extensions.begin(), instance_extensions.end(), [name](const auto& extension) {
			return std::strcmp(extension.extensionName, name) == 0;
		});
	};

	constexpr char khr_swapchain_maintenance[] = "VK_KHR_swapchain_maintenance1";
	constexpr char ext_surface_maintenance[] = "VK_EXT_surface_maintenance1";
	constexpr char khr_surface_maintenance[] = "VK_KHR_surface_maintenance1";
	if (has_device_extension(khr_swapchain_maintenance) && has_instance_extension(khr_surface_maintenance))
		return khr_swapchain_maintenance;
	if (has_device_extension(VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME)
		&& has_instance_extension(ext_surface_maintenance))
		return VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME;
	return nullptr;
}

bool GraphicsEngineDevice::supports_required_features(VkPhysicalDevice device)
{
	VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT swapchain_maintenance1{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT};
	VkPhysicalDeviceFeatures2 supported_features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
	supported_features.pNext = &swapchain_maintenance1;
	vkGetPhysicalDeviceFeatures2(device, &supported_features);
	return swapchain_maintenance1.swapchainMaintenance1 == VK_TRUE;
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
	const auto required_device_extensions = get_required_extensions(swapchain_maintenance_extension);
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

	// For ray tracing:
	// LOG_INFO(Utility::get_logger(),
	// 		 "Physical Device Properties: maxBoundDescriptorSets: {}, maxMSAA_Samples: {}, "
	// 		 "maxRayRecursionDepth: {}, minStorageBufferOffsetAlignment: {}",
	// 		 properties.properties.limits.maxBoundDescriptorSets,
	// 		 int(get_max_usable_msaa()),
	// 		 ray_tracing_properties.maxRayRecursionDepth,
	// 		 size_t(properties.properties.limits.minStorageBufferOffsetAlignment));
	LOG_INFO(Utility::get_logger(),
			 "Physical Device Properties: maxBoundDescriptorSets: {}, maxMSAA_Samples: {}, "
			 "minStorageBufferOffsetAlignment: {}",
			 properties.properties.limits.maxBoundDescriptorSets,
			 int(get_max_usable_msaa()),
			 size_t(properties.properties.limits.minStorageBufferOffsetAlignment));
}

const VkPhysicalDeviceProperties2& GraphicsEngineDevice::get_physical_device_properties()
{
	if (!physical_device_properties)
	{
		physical_device_properties = VkPhysicalDeviceProperties2{};
		physical_device_properties->sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		// For ray tracing:
		// physical_device_properties->pNext = &ray_tracing_properties;
		vkGetPhysicalDeviceProperties2(get_physical_device(), &physical_device_properties.value());
	}

	return physical_device_properties.value();
}

// For ray tracing:
// VkDeviceAddress GraphicsEngineDevice::get_buffer_device_address(VkBuffer buffer)
// {
// 	VkBufferDeviceAddressInfo info{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
// 	info.buffer = buffer;
// 	return vkGetBufferDeviceAddress(get_logical_device(), &info);
// }

std::vector<const char*> GraphicsEngineDevice::get_required_extensions(const char* maintenance_extension)
{
	std::vector<const char*> required_device_extensions;

	required_device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
	if (maintenance_extension != nullptr)
		required_device_extensions.push_back(maintenance_extension);
	// Ray-tracing extensions are intentionally not requested while unsupported.
	// For ray tracing:
	// required_device_extensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
	// required_device_extensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
	// required_device_extensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);

#ifdef _DEBUG
	// TODO: enable if want shader printf
	// required_device_extensions.push_back(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME);
#endif

	return required_device_extensions;
}

VkPhysicalDeviceFeatures2* GraphicsEngineDevice::get_required_features()
{
	// For ray tracing:
	// static VkPhysicalDeviceAccelerationStructureFeaturesKHR acceleration_structure_features{
	// 	VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
	// acceleration_structure_features.accelerationStructure = true;
	//
	// static VkPhysicalDeviceRayTracingPipelineFeaturesKHR ray_tracing_pipeline_features{
	// 	VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR};
	// ray_tracing_pipeline_features.rayTracingPipeline = true;
	//
	// static VkPhysicalDeviceVulkan12Features device_features12{
	// 	VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
	// device_features12.bufferDeviceAddress = true;

	// special features, we request
	static VkPhysicalDeviceFeatures2 device_features2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
	device_features2.features.samplerAnisotropy = true;
	device_features2.features.fillModeNonSolid = true;
	device_features2.features.geometryShader = true;
	device_features2.features.imageCubeArray = true;
	device_features2.features.textureCompressionBC = true;
	static VkPhysicalDeviceVulkan13Features vulkan13{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
	vulkan13.dynamicRendering = VK_TRUE;
	vulkan13.synchronization2 = VK_TRUE;
	static VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT swapchain_maintenance1{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT};
	swapchain_maintenance1.swapchainMaintenance1 = VK_TRUE;
	vulkan13.pNext = &swapchain_maintenance1;
	device_features2.pNext = &vulkan13;

	// For ray tracing:
	// device_features2.pNext = &device_features12;
	// device_features12.pNext = &acceleration_structure_features;
	// acceleration_structure_features.pNext = &ray_tracing_pipeline_features;

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
