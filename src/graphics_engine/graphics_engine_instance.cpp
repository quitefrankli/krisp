#include "graphics_engine.hpp"
#include "graphics_engine_instance.hpp"
#include "graphics_engine_validation_layer.hpp"
#include "utility_functions.hpp"

#include <GLFW/glfw3.h>

#include <iostream>


std::vector<std::string> GraphicsEngineInstance::get_required_extensions() const
{
	uint32_t glfwExtensionCount;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	auto required_extensions = char_ptr_arr_to_str_vec(glfwExtensions, glfwExtensionCount);
	if (GraphicsEngineValidationLayer::is_enabled())
	{
		required_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		required_extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
	}

	return required_extensions;
}

GraphicsEngineInstance::GraphicsEngineInstance(GraphicsEngine& engine) :
	GraphicsEngineBaseModule(engine)
{
	VkApplicationInfo app_info{};
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pApplicationName = APPLICATION_NAME.c_str();
	app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.pEngineName = ENGINE_NAME.c_str();
	app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.pApplicationInfo = &app_info;

	auto validation_layers = GraphicsEngineValidationLayer::get_layers();
	create_info.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
	create_info.ppEnabledLayerNames = validation_layers.data();

	// validation layer stuff
	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = GraphicsEngineValidationLayer::get_messenger_create_info();
	create_info.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;

	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
	std::vector<VkExtensionProperties> extensions(extensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
	std::cout << "Available extensions:\n";
	for (const auto& extension : extensions) {
		std::cout << '\t' << extension.extensionName << '\n';
	}

	// vulkan is platform agnostic and therefore an extension is necessary 
	std::vector<std::string> required_extensions = get_required_extensions();
	std::cout << "Required extensions:\n";
	for (auto& required_extension : required_extensions)
	{
		std::cout << '\t' << required_extension << '\n';
	}
	auto required_extensions_old = c_style_str_array(required_extensions);
	create_info.enabledExtensionCount = required_extensions_old.size();
	create_info.ppEnabledExtensionNames = required_extensions_old.data();

	if (vkCreateInstance(&create_info, nullptr, &instance) != VK_SUCCESS) {
		throw std::runtime_error("failed to create instance!");
	}

	if (glfwCreateWindowSurface(get_instance(), get_graphics_engine().get_window(), nullptr, &window_surface) != VK_SUCCESS) // create window surface
	{
		throw std::runtime_error("failed to create window surface!");
	}
}

GraphicsEngineInstance::~GraphicsEngineInstance()
{
	vkDestroySurfaceKHR(get_instance(), window_surface, nullptr);

	vkDestroyInstance(instance, nullptr);
}