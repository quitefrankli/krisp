#pragma once

#include "graphics_engine_base_module.hpp"


template<typename GraphicsEngineT>
GraphicsEngineBaseModule<GraphicsEngineT>::GraphicsEngineBaseModule(GraphicsEngineT& engine) :
	graphics_engine(engine)
{
}

template<typename GraphicsEngineT>
VkDevice& GraphicsEngineBaseModule<GraphicsEngineT>::get_logical_device()
{
	return graphics_engine.get_logical_device();
}

template<typename GraphicsEngineT>
VkPhysicalDevice& GraphicsEngineBaseModule<GraphicsEngineT>::get_physical_device()
{
	return graphics_engine.get_physical_device();
}

template<typename GraphicsEngineT>
VkInstance& GraphicsEngineBaseModule<GraphicsEngineT>::get_instance()
{
	return graphics_engine.get_instance();
}

template<typename GraphicsEngineT>
void GraphicsEngineBaseModule<GraphicsEngineT>::create_buffer(size_t size, 
											 VkBufferUsageFlags usage_flags, 
											 VkMemoryPropertyFlags memory_flags, 
											 VkBuffer& buffer, 
											 VkDeviceMemory& device_memory)
{
	graphics_engine.create_buffer(size, usage_flags, memory_flags, buffer, device_memory);
}

template<typename GraphicsEngineT>
uint32_t GraphicsEngineBaseModule<GraphicsEngineT>::get_num_swapchain_frames() const
{
	return graphics_engine.get_num_swapchain_images();
}
