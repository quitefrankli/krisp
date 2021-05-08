#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <optional>
#include <set>

#include "graphics_engine_validation_layer.hpp"
#include "queues.hpp"
#include "utility_functions.hpp"


struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

class GraphicsEngine {
public:
	void run() {
		initWindow();
		initVulkan();
		mainLoop();
		cleanup();
	}

private:
	GLFWwindow* window;
	// the instance is the connection between application and Vulkan library
	// its creation involves specifying some details about your application to the driver
	VkInstance instance; 
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice logical_device;
	VkQueue graphics_queue;
	VkQueue present_queue;
	VkSurfaceKHR window_surface;
	VkSwapchainKHR swap_chain;
	std::vector<VkImage> swap_chain_images; // handle of the swap chain images
	VkFormat swap_chain_image_format;
	VkExtent2D swap_chain_extent;
	std::vector<VkImageView> swap_chain_image_views;
	VkDebugUtilsMessengerEXT debug_messenger;
	const uint32_t WIDTH = 800;
	const uint32_t HEIGHT = 600;
	// swap chain
	const std::vector<std::string> device_extensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    void initWindow() {
		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
		window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    }
    void initVulkan() {
		createInstance();
		create_window_surface();
		setupDebugMessenger();
		pick_physical_device();
		create_logical_device();
		create_swap_chain();
		create_image_views();
    }

	void createInstance() 
	{
		VkApplicationInfo app_info{};
		app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		app_info.pApplicationName = "Hello Triangle";
		app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		app_info.pEngineName = "No Engine";
		app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		app_info.apiVersion = VK_API_VERSION_1_0;

		VkInstanceCreateInfo create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		create_info.pApplicationInfo = &app_info;
		if (enableValidationLayers) { // see validation_layer.hpp
			if (!checkValidationLayerSupport()) {
				throw std::runtime_error("validation layers requested, but not available!");
			}
			create_info.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			create_info.ppEnabledLayerNames = validationLayers.data();
		} else {
			create_info.enabledLayerCount = 0;
		}

		// validation layer stuff
		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
		populateDebugMessengerCreateInfo(debugCreateInfo);
		create_info.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;

		// vulkan is platform agnostic and therefore an extension is necessary 
		std::vector<std::string> required_extensions = get_required_extensions();
		std::cout << "required extensions:\n";
		for (auto& required_extension : required_extensions)
		{
			std::cout << '\t' << required_extension << '\n';
		}
		auto required_extensions_old = c_style_str_array(required_extensions);
		create_info.enabledExtensionCount = required_extensions_old.size();
		create_info.ppEnabledExtensionNames = required_extensions_old.data();
		
		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
		std::vector<VkExtensionProperties> extensions(extensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
		std::cout << "available extensions:\n";
		for (const auto& extension : extensions) {
			std::cout << '\t' << extension.extensionName << '\n';
		}

		VkResult result = vkCreateInstance(&create_info, nullptr, &instance);
		if (vkCreateInstance(&create_info, nullptr, &instance) != VK_SUCCESS) {
 		   	throw std::runtime_error("failed to create instance!");
		}
	}

	void create_window_surface()
	{
		if (glfwCreateWindowSurface(instance, window, nullptr, &window_surface) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create window surface!");
		}
	}

	void pick_physical_device()
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

public: // swap chain
	void create_swap_chain();
	SwapChainSupportDetails query_swap_chain_support(VkPhysicalDevice device);
	VkSurfaceFormatKHR choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR>& available_formats);
	VkPresentModeKHR choose_swap_present_mode(const std::vector<VkPresentModeKHR>& available_present_modes);
	// extent = resolution of the swap chain images and ~ resolution of window we are drawing to
	VkExtent2D choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities);

public: // image views
	void create_image_views();

	bool check_device_extension_support(VkPhysicalDevice device, std::vector<std::string> device_extensions)
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

	void create_logical_device()
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
		create_info.enabledExtensionCount = device_extensions.size();
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

	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
		QueueFamilyIndices indices;
		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		for (int i = 0; i < queueFamilies.size(); i++)
		{
			if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				indices.graphicsFamily = i;
			}
			VkBool32 present_support = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, window_surface, &present_support);
			if (present_support)
			{
				indices.presentFamily = i;
			}
		}

		return indices;
	}

	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
	void setupDebugMessenger();
	VkResult CreateDebugUtilsMessengerEXT(
		VkInstance instance, 
		const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, 
		const VkAllocationCallbacks* pAllocator, 
		VkDebugUtilsMessengerEXT* pDebugMessenger);
	void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);
	
	std::vector<std::string> get_required_extensions()
	{
		uint32_t glfwExtensionCount;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		auto required_extensions = char_ptr_arr_to_str_vec(glfwExtensions, glfwExtensionCount);
		if (enableValidationLayers)
		{
			required_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}
		return required_extensions;
	}

    void mainLoop() {
		while (!glfwWindowShouldClose(window)) {
			glfwPollEvents();
		}
    }

    void cleanup() {
		std::cout<<"cleaning up\n";
		for (auto& image_view : swap_chain_image_views)
		{
			vkDestroyImageView(logical_device, image_view, nullptr);
		}
		vkDestroySwapchainKHR(logical_device, swap_chain, nullptr);
		vkDestroyDevice(logical_device, nullptr);
		vkDestroySurfaceKHR(instance, window_surface, nullptr);
		if (enableValidationLayers)
		{
			DestroyDebugUtilsMessengerEXT(instance, debug_messenger, nullptr);
		}
		vkDestroyInstance(instance, nullptr);
		glfwDestroyWindow(window);
   	 	glfwTerminate();
    }
};