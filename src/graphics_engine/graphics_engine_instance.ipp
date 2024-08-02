#pragma once

#include "graphics_engine_instance.hpp"
#include "utility.hpp"

#include <quill/LogMacros.h>
#include <GLFW/glfw3.h>


GraphicsEngineInstance::GraphicsEngineInstance(GraphicsEngine& engine) :
	GraphicsEngineBaseModule(engine)
{
	VkApplicationInfo app_info{VK_STRUCTURE_TYPE_APPLICATION_INFO};
	app_info.pApplicationName = APPLICATION_NAME.c_str();
	app_info.applicationVersion = VK_MAKE_VERSION(1, 3, 211); // i think this is application specific, so might not be necessary
	app_info.pEngineName = ENGINE_NAME.c_str();
	app_info.engineVersion = VK_MAKE_VERSION(1, 3, 211); // needs to be associated with vulkan version
	app_info.apiVersion = VK_API_VERSION_1_3;

	VkInstanceCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.pApplicationInfo = &app_info;

	const std::vector<const char*> required_layers = GraphicsEngineValidationLayer::get_layers();
	create_info.enabledLayerCount = static_cast<uint32_t>(required_layers.size());
	create_info.ppEnabledLayerNames = required_layers.data();

	// validation layer stuff
	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = GraphicsEngineValidationLayer::get_messenger_create_info();
	create_info.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;

#ifndef NDEBUG
	// TODO: enable if want shader debug printf
	// std::vector<VkValidationFeatureEnableEXT> enables = {VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT};
	// VkValidationFeaturesEXT features{VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT};
	// features.enabledValidationFeatureCount = enables.size();
	// features.pEnabledValidationFeatures = enables.data();
	// debugCreateInfo.pNext = &features;
#endif

	const auto available_instance_extensions = [&]() {
		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
		std::vector<VkExtensionProperties> extensions(extensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
		std::vector<std::string> extensions_str;
		for (const auto& extension : extensions) {
			extensions_str.push_back(extension.extensionName);
		}
		return extensions_str;
	}();

	LOG_INFO(Utility::get_logger(), 
			 "GraphicsEngineInstance: available extensions:={}", 
			 fmt::format("{}", fmt::join(available_instance_extensions, ", ")));

	// vulkan is platform agnostic and therefore an extension is necessary 
	std::vector<std::string> required_extensions = get_required_extensions();
	LOG_INFO(Utility::get_logger(), 
			 "GraphicsEngineInstance: required extensions:={}", 
			 fmt::format("{}", fmt::join(required_extensions, ", ")));

	// convert vector of string to vector const char pointer
	std::vector<const char*> required_extensions_old;
	for (const auto& extension : required_extensions)
	{
		required_extensions_old.push_back(extension.c_str());
	}

	create_info.enabledExtensionCount = required_extensions_old.size();
	create_info.ppEnabledExtensionNames = required_extensions_old.data();

#ifdef __APPLE__
	create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

	if (vkCreateInstance(&create_info, nullptr, &instance) != VK_SUCCESS) {
		throw std::runtime_error("failed to create instance!");
	}

	if (glfwCreateWindowSurface(
			get_instance(), 
			get_graphics_engine().get_window().get_glfw_window(), 
			nullptr, 
			&window_surface) != VK_SUCCESS) // create window surface
	{
		throw std::runtime_error("failed to create window surface!");
	}
}
	
GraphicsEngineInstance::~GraphicsEngineInstance()
{
	vkDestroySurfaceKHR(get_instance(), window_surface, nullptr);

	vkDestroyInstance(instance, nullptr);
}

std::vector<std::string> GraphicsEngineInstance::get_required_extensions() const
{
	uint32_t glfwExtensionCount;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	if (glfwExtensionCount == 0 || glfwExtensions == nullptr)
	{
		throw std::runtime_error("GraphicsEngineInstance: no glfw extensions found!");
	}

	std::vector<std::string> required_extensions;
	for (int i = 0; i < glfwExtensionCount; ++i)
	{
		required_extensions.push_back(glfwExtensions[i]);
	}

	if (GraphicsEngineValidationLayer::is_enabled())
	{
		required_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		required_extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
	}
#ifdef __APPLE__
	required_extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
#endif

	return required_extensions;
}