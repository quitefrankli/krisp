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

void frame_buffer_resize_callback(GLFWwindow* window, int width, int height);

class GraphicsEngine {
public:
	void run() {
		initWindow();
		initVulkan();
		mainLoop();
		cleanup();
	}

	void set_frame_buffer_resized()
	{
		frame_buffer_resized = true;
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
	VkPipelineLayout pipeline_layout;
	std::vector<VkImageView> swap_chain_image_views;
	VkRenderPass render_pass;
	VkPipeline graphics_engine_pipeline;
	std::vector<VkFramebuffer> swap_chain_frame_buffers;
	VkCommandPool command_pool;
	std::vector<VkCommandBuffer> command_buffers;
	std::vector<VkSemaphore> image_available_semaphores;
	std::vector<VkSemaphore> render_finished_semaphores;
	std::vector<VkFence> in_flight_fences;
	std::vector<VkFence> images_in_flight;
	VkDebugUtilsMessengerEXT debug_messenger;
	const uint32_t WIDTH = 800;
	const uint32_t HEIGHT = 600;
	// swap chain
	const std::vector<std::string> device_extensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	int current_frame = 0;
	bool frame_buffer_resized = false;
	const int MAX_FRAMES_IN_FLIGHT = 2;

    void initWindow() {
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		// glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
		window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
		glfwSetWindowUserPointer(window, this);
		glfwSetFramebufferSizeCallback(window, frame_buffer_resize_callback);
    }
    
	void initVulkan() {
		createInstance();
		create_window_surface();
		setupDebugMessenger();
		pick_physical_device();
		create_logical_device();
		create_swap_chain();
		create_image_views();
		create_render_pass();
		create_graphics_pipeline();
		create_frame_buffers();
		create_command_pool();
		create_command_buffers();
		create_synchronisation_objects();
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
		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
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

	void create_synchronisation_objects() // semaphores and fences, for GPU-GPU and CPU-GPU synchronisation
	{
		image_available_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
		render_finished_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
		in_flight_fences.resize(MAX_FRAMES_IN_FLIGHT);
		images_in_flight.resize(swap_chain_images.size(), VK_NULL_HANDLE);

		VkSemaphoreCreateInfo semaphore_create_info{};
		semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fence_create_info{};
		fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			if (vkCreateSemaphore(logical_device, &semaphore_create_info, nullptr, &image_available_semaphores[i]) != VK_SUCCESS ||
				vkCreateSemaphore(logical_device, &semaphore_create_info, nullptr, &render_finished_semaphores[i]) != VK_SUCCESS ||
				vkCreateFence(logical_device, &fence_create_info, nullptr, &in_flight_fences[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create semaphores!");
			}
		}
	}

// if confused about the different vulkan definitions see here
// https://stackoverflow.com/questions/39557141/what-is-the-difference-between-framebuffer-and-image-in-vulkan

public: // swap chain
	void create_swap_chain();
	SwapChainSupportDetails query_swap_chain_support(VkPhysicalDevice device);
	VkSurfaceFormatKHR choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR>& available_formats);
	VkPresentModeKHR choose_swap_present_mode(const std::vector<VkPresentModeKHR>& available_present_modes);
	// extent = resolution of the swap chain images and ~ resolution of window we are drawing to
	VkExtent2D choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities);

	void recreate_swap_chain(); // useful for when size of window is changing
	void clean_up_swap_chain();

public: // image views
	void create_image_views();

public: // graphics pipeline
	void create_graphics_pipeline();
	void create_render_pass();

public: // frame buffer
	void create_frame_buffers();

public: // command buffer
	void create_command_pool();
	void create_command_buffers();

public: // validation layer
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

	void draw_frame()
	{
		// 1. acquire image from swap chain
		// 2. execute command buffer with image as attachment in the frame buffer
		// 3. return the image to swap chain for presentation

		// CPU-GPU synchronisation for in flight images
		vkWaitForFences(logical_device, 1, &in_flight_fences[current_frame], VK_TRUE, std::numeric_limits<uint64_t>::max());

		unsigned image_index;
		// using uint64 max disables timeout
		VkResult result = vkAcquireNextImageKHR(logical_device, swap_chain, std::numeric_limits<uint64_t>::max(), 
			image_available_semaphores[current_frame], VK_NULL_HANDLE, &image_index);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreate_swap_chain();
            return;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

		// check if a previous frame is using this image (i.e. there is it fence to wait on)
		if (images_in_flight[image_index] != VK_NULL_HANDLE)
		{
			vkWaitForFences(logical_device, 1, &images_in_flight[image_index], VK_TRUE, std::numeric_limits<uint64_t>::max());
		}
		// mark the image as now being in use by this frame
		images_in_flight[image_index] = in_flight_fences[current_frame];		

		//
		// submitting the command buffer
		//

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore waitSemaphores[] = { image_available_semaphores[current_frame] };
		VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
		// here we specify which semaphore to wait on before execution begins and in which stage of the pipeline to wait
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;

		// here we specify which command buffers to actually submit for execution
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &command_buffers[image_index];

		// here we specify the semaphores to signal once the command buffer has finished execution
		VkSemaphore signal_semaphores[] = { render_finished_semaphores[current_frame] };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signal_semaphores;

		vkResetFences(logical_device, 1, &in_flight_fences[current_frame]);
		if (vkQueueSubmit(graphics_queue, 1, &submitInfo, in_flight_fences[current_frame]) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to submit draw command buffer!");
		}

		//
		// presentation
		//

		VkPresentInfoKHR present_info{};
		present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		present_info.waitSemaphoreCount = 1;
		present_info.pWaitSemaphores = signal_semaphores;

		VkSwapchainKHR swap_chains[] = { swap_chain };
		present_info.swapchainCount = 1;
		present_info.pSwapchains = swap_chains;
		present_info.pImageIndices = &image_index;
		present_info.pResults = nullptr; // allows you to specify array of VkResult values to check for every individual swap chain if presentation was successful

		current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;

		result = vkQueuePresentKHR(present_queue, &present_info);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || frame_buffer_resized) {
            frame_buffer_resized = false;
            recreate_swap_chain();
        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }
	}

    void mainLoop() {
		while (!glfwWindowShouldClose(window)) {
			glfwPollEvents();
			draw_frame();
		}
    }

    void cleanup() {
		std::cout<<"cleaning up\n";
		vkDeviceWaitIdle(logical_device);

		clean_up_swap_chain();
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			vkDestroySemaphore(logical_device, image_available_semaphores[i], nullptr);
			vkDestroySemaphore(logical_device, render_finished_semaphores[i], nullptr);
			vkDestroyFence(logical_device, in_flight_fences[i], nullptr);
		}
		vkDestroyCommandPool(logical_device, command_pool, nullptr);

		vkDestroyDevice(logical_device, nullptr);
		if (enableValidationLayers)
		{
			DestroyDebugUtilsMessengerEXT(instance, debug_messenger, nullptr);
		}
		vkDestroySurfaceKHR(instance, window_surface, nullptr);
		vkDestroyInstance(instance, nullptr);
		glfwDestroyWindow(window);
   	 	glfwTerminate();
    }
};

static void frame_buffer_resize_callback(GLFWwindow* window, int width, int height)
{
	auto app = reinterpret_cast<GraphicsEngine*>(glfwGetWindowUserPointer(window));
	app->set_frame_buffer_resized();
}