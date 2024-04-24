/*
	Due to the limited amount of handholding vulkan provides, we would like 
	a layer of validation between our API calls and the vulkan library
*/

#pragma once

#include "graphics_engine_base_module.hpp"

#include <vulkan/vulkan.hpp>

#include <vector>
#include <string>


class GraphicsEngineValidationLayer : public GraphicsEngineBaseModule
{
private:
	const static std::vector<const char*> REQUIRED_VALIDATION_LAYERS;

public:
	GraphicsEngineValidationLayer(GraphicsEngine& engine);
	~GraphicsEngineValidationLayer();

	static constexpr bool is_enabled()
	{
		return
			#ifdef NDEBUG
				false;
			#else
				true;
			#endif
	}
	static bool check_validation_layer_support(bool print_support = false);
	static std::vector<const char*> get_layers();
	static VkDebugUtilsMessengerCreateInfoEXT get_messenger_create_info();

private:
		
	VkDebugUtilsMessengerEXT debug_messenger;
};