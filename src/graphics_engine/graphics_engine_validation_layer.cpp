#include "graphics_engine_validation_layer.hpp"

#include <vulkan/vulkan.hpp>

#include <vector>
#include <iostream>
#include <unordered_set>


const std::vector<const char*> GraphicsEngineValidationLayer::REQUIRED_VALIDATION_LAYERS = {
	"VK_LAYER_KHRONOS_validation"
};

GraphicsEngineValidationLayer::GraphicsEngineValidationLayer(GraphicsEngine& engine) :
	GraphicsEngineBaseModule(engine)
{
	if (!is_enabled())
	{
		return;
	}

	VkDebugUtilsMessengerCreateInfoEXT createInfo = get_messenger_create_info();

	// loads up function from dynamic library
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(get_instance(), "vkCreateDebugUtilsMessengerEXT");
	if (!func || func(get_instance(), &createInfo, nullptr, &debug_messenger) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to set up debug messenger!");
	}
}

GraphicsEngineValidationLayer::~GraphicsEngineValidationLayer()
{
	// loads up function from dynamic library
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(get_instance(), "vkDestroyDebugUtilsMessengerEXT");
	if (!func)
	{
		throw std::runtime_error("failed to destroy debug messenger!");
	}

	func(get_instance(), debug_messenger, nullptr);
}

std::vector<const char*> GraphicsEngineValidationLayer::get_layers()
{
	if (!is_enabled())
	{
		return std::vector<const char*>{};
	}

	if (!check_validation_layer_support()) {
		throw std::runtime_error("validation layers requested, but not available!");
	}

	return REQUIRED_VALIDATION_LAYERS;
}

bool GraphicsEngineValidationLayer::check_validation_layer_support()
{
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	std::unordered_set<std::string> available_layers_set; // for hash table

	std::cout << "Available Validation Layers:\n";
	std::for_each(availableLayers.begin(), availableLayers.end(), [&available_layers_set](const auto& layer)
		{
			printf("\t%s - %s\n", layer.layerName, layer.description);
			available_layers_set.emplace(layer.layerName);
		});

	std::cout << "Required Valiation Layers:\n";
	for (const char* layer : REQUIRED_VALIDATION_LAYERS)
	{
		printf("\t%s\n", layer);
		if (available_layers_set.find(std::string(layer)) == available_layers_set.end())
		{
			return false;
		}
	}

	return true;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData) 
{
	std::string error(pCallbackData->pMessage);
	if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT &&
		error.find("Epic Games") == std::string::npos)
	{
		std::cerr << "validation layer: " << error << std::endl;
	}

	return VK_FALSE;
}

VkDebugUtilsMessengerCreateInfoEXT GraphicsEngineValidationLayer::get_messenger_create_info()
{
	VkDebugUtilsMessengerCreateInfoEXT createInfo{};

	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = debug_callback;

	return createInfo;
}