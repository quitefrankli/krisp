#pragma once

#include "graphics_engine_base_module.hpp"

#include <vulkan/vulkan.hpp>

#include <string>
#include <vector>


// the instance is the connection between application and Vulkan library
// its creation involves specifying some details about your application to the driver
template<typename GraphicsEngineT>
class GraphicsEngineInstance : public GraphicsEngineBaseModule<GraphicsEngineT>
{
public:
	GraphicsEngineInstance(GraphicsEngineT& engine);
	~GraphicsEngineInstance();

	VkInstance& get() { return instance; }
	VkSurfaceKHR window_surface;

private:
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_graphics_engine;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_logical_device;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_physical_device;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_instance;
	using GraphicsEngineBaseModule<GraphicsEngineT>::create_buffer;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_num_swapchain_frames;
	
	const std::string APPLICATION_NAME = "My Application";
	const std::string ENGINE_NAME = "My Engine";

	std::vector<std::string> get_required_extensions() const;

	VkInstance instance;
};