/*
	Due to the limited amount of handholding vulkan provides, we would like 
	a layer of validation between our API calls and the vulkan library
*/

#pragma once

#include "graphics_engine_base_module.hpp"

#include <vulkan/vulkan.hpp>

#include <vector>
#include <string>


template<typename GraphicsEngineT>
class GraphicsEngineValidationLayer : public GraphicsEngineBaseModule<GraphicsEngineT>
{
private:
	const static std::vector<const char*> REQUIRED_VALIDATION_LAYERS;

public:
	GraphicsEngineValidationLayer(GraphicsEngineT& engine);
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
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_graphics_engine;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_logical_device;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_physical_device;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_instance;
	using GraphicsEngineBaseModule<GraphicsEngineT>::create_buffer;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_num_swapchain_frames;
		
	VkDebugUtilsMessengerEXT debug_messenger;
};