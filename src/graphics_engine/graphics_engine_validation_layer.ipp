#pragma once

#include "graphics_engine_validation_layer.hpp"
#include "utility.hpp"

#include <vulkan/vulkan.hpp>
#include <quill/LogMacros.h>

#include <vector>
#include <iostream>
#include <unordered_set>


static uint32_t filtered_errors_count = 0;

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

	check_validation_layer_support(true);

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
	if (func)
	{
		func(get_instance(), debug_messenger, nullptr);
	} else {
		// throw std::runtime_error("failed to destroy debug messenger!");
		// I think there might be a bug here, the library seems to get unloaded as soon as this thread ends
	}

	LOG_INFO(Utility::get_logger(), "GraphicsEngineValidationLayer: num validation errors omitted:={}", filtered_errors_count);
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

bool GraphicsEngineValidationLayer::check_validation_layer_support(bool print_support)
{
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	std::unordered_set<std::string> available_layers_set; // for hash table

	if (print_support)
	{
		std::for_each(availableLayers.begin(), availableLayers.end(), [&available_layers_set](const auto& layer)
			{
				LOG_INFO(Utility::get_logger(), "GraphicsEngineValidationLayer: available layer: {} - {}", layer.layerName, layer.description);
				available_layers_set.emplace(layer.layerName);
			});

		for (const char* layer : REQUIRED_VALIDATION_LAYERS)
		{
			LOG_INFO(Utility::get_logger(), "GraphicsEngineValidationLayer: required layer: {}", layer);
			if (available_layers_set.find(std::string(layer)) == available_layers_set.end())
			{
				return false;
			}
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
	constexpr uint32_t MAX_NUM_ERRORS_BEFORE_THROW = 10;
	static uint32_t num_errors = 0;

	const std::string_view error(pCallbackData->pMessage);
	const std::vector<std::string_view> blacklist_filter = {
		"Epic Games"
	};
	if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT &&
		std::all_of(blacklist_filter.begin(), blacklist_filter.end(), [&error](const auto& filter) {
			return error.find(filter) == std::string::npos;
		}))
	{
		LOG_ERROR(Utility::get_logger(), "ValidationLayerMessage: {}", error);

		if (num_errors++ > MAX_NUM_ERRORS_BEFORE_THROW)
		{
			throw std::runtime_error("ValidationLayer: too many errors!");
		}
	} else
	{
		++filtered_errors_count;
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