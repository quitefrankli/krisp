#pragma once

#include "graphics_engine_base_module.hpp"
#include "utility.hpp"

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
#include <quill/Quill.h>

#include <string>
#include <vector>
#include <iostream>


// the instance is the connection between application and Vulkan library
// its creation involves specifying some details about your application to the driver
template<typename GraphicsEngineT>
class GraphicsEngineInstance : public GraphicsEngineBaseModule<GraphicsEngineT>
{
public:
	GraphicsEngineInstance(GraphicsEngineT& engine) :
		GraphicsEngineBaseModule<GraphicsEngineT>(engine)
	{
		VkApplicationInfo app_info{};
		app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		app_info.pApplicationName = APPLICATION_NAME.c_str();
		app_info.applicationVersion = VK_MAKE_VERSION(1, 2, 172); // i think this is application specific, so might not be necessary
		app_info.pEngineName = ENGINE_NAME.c_str();
		app_info.engineVersion = VK_MAKE_VERSION(1, 2, 172); // needs to be associated with vulkan version
		app_info.apiVersion = VK_API_VERSION_1_2;

		VkInstanceCreateInfo create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		create_info.pApplicationInfo = &app_info;

		const std::vector<const char*> required_layers = GraphicsEngineValidationLayer<GraphicsEngineT>::get_layers();
		create_info.enabledLayerCount = static_cast<uint32_t>(required_layers.size());
		create_info.ppEnabledLayerNames = required_layers.data();

		// validation layer stuff
		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = GraphicsEngineValidationLayer<GraphicsEngineT>::get_messenger_create_info();
		create_info.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;

		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
		std::vector<VkExtensionProperties> extensions(extensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
		for (const auto& extension : extensions) {
			LOG_INFO(Utility::get().get_logger(), "GraphicsEngineInstance: available instance: {}", extension.extensionName);
		}

		// vulkan is platform agnostic and therefore an extension is necessary 
		std::vector<std::string> required_extensions = get_required_extensions();
		for (auto& required_extension : required_extensions)
		{
			LOG_INFO(Utility::get().get_logger(), "GraphicsEngineInstance: required extension: {}", required_extension);
		}
		auto required_extensions_old = c_style_str_array(required_extensions);
		create_info.enabledExtensionCount = required_extensions_old.size();
		create_info.ppEnabledExtensionNames = required_extensions_old.data();
		create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

		if (vkCreateInstance(&create_info, nullptr, &instance) != VK_SUCCESS) {
			throw std::runtime_error("failed to create instance!");
		}

		if (glfwCreateWindowSurface(get_instance(), get_graphics_engine().get_window(), nullptr, &window_surface) != VK_SUCCESS) // create window surface
		{
			throw std::runtime_error("failed to create window surface!");
		}
	}
	
	~GraphicsEngineInstance()
	{
		vkDestroySurfaceKHR(get_instance(), window_surface, nullptr);

		vkDestroyInstance(instance, nullptr);
	}

	VkInstance& get() { return instance; }
	VkSurfaceKHR window_surface;

private:
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_graphics_engine;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_logical_device;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_physical_device;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_instance;
	using GraphicsEngineBaseModule<GraphicsEngineT>::create_buffer;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_num_swapchain_frames;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_render_pass;
	
	const std::string APPLICATION_NAME = "My Application";
	const std::string ENGINE_NAME = "My Engine";

	std::vector<std::string> get_required_extensions() const
	{
		uint32_t glfwExtensionCount;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
		if (glfwExtensionCount == 0 || glfwExtensions == nullptr)
		{
			throw std::runtime_error("GraphicsEngineInstance: no glfw extensions found!");
		}

		std::vector<std::string> required_extensions = char_ptr_arr_to_str_vec(glfwExtensions, glfwExtensionCount);
		if (GraphicsEngineValidationLayer<GraphicsEngineT>::is_enabled())
		{
			required_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			required_extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
		}
#ifdef __APPLE__
		required_extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
#endif
		return required_extensions;
	}

	VkInstance instance;
};