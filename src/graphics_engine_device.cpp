#include "graphics_engine.hpp"

void GraphicsEngine::pick_physical_device()
{
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
	if (deviceCount == 0) {
		throw std::runtime_error("failed to find GPUs with Vulkan support!");
	}
	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

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

		if (!findQueueFamilies(device).isComplete())
		{
			return false;
		}

		SwapChainSupportDetails swap_chain_support = this->query_swap_chain_support(device);
		if (swap_chain_support.formats.empty() || swap_chain_support.presentModes.empty())
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

bool GraphicsEngine::check_device_extension_support(VkPhysicalDevice device, std::vector<std::string> device_extensions)
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

void GraphicsEngine::create_logical_device()
{
	QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

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
	VkPhysicalDeviceFeatures deviceFeatures{}; // special features, don't need atm
	
	VkDeviceCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	create_info.pQueueCreateInfos = queue_create_infos.data();
	create_info.queueCreateInfoCount = queue_create_infos.size();
	create_info.pEnabledFeatures = &deviceFeatures;
	create_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());
	c_style_str_array c_style_device_extensions(device_extensions);
	create_info.ppEnabledExtensionNames = c_style_device_extensions.data();
	if (enableValidationLayers)
	{
		create_info.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		create_info.ppEnabledLayerNames = validationLayers.data();
	} else {
		create_info.enabledLayerCount = 0;
	}

	if (vkCreateDevice(physicalDevice, &create_info, nullptr, &logical_device) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create logical device!");
	}

	// retrieves the queue handles
	vkGetDeviceQueue(logical_device, indices.presentFamily.value(), 0, &present_queue);
	vkGetDeviceQueue(logical_device, indices.graphicsFamily.value(), 0, &graphics_queue);
}