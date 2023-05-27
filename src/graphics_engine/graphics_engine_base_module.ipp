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
GraphicsResourceManager<GraphicsEngineT>& GraphicsEngineBaseModule<GraphicsEngineT>::get_rsrc_mgr()
{
	return graphics_engine.get_rsrc_mgr();
}

template<typename GraphicsEngineT>
const GraphicsResourceManager<GraphicsEngineT>& GraphicsEngineBaseModule<GraphicsEngineT>::get_rsrc_mgr() const
{
	return graphics_engine.get_rsrc_mgr();
}

template<typename GraphicsEngineT>
GraphicsBuffer GraphicsEngineBaseModule<GraphicsEngineT>::create_buffer(
	size_t size, 
	VkBufferUsageFlags usage_flags, 
	VkMemoryPropertyFlags memory_flags)
{
	return graphics_engine.get_rsrc_mgr().create_buffer(size, usage_flags, memory_flags);
}

template<typename GraphicsEngineT>
VkDeviceAddress GraphicsEngineBaseModule<GraphicsEngineT>::get_buffer_device_address(
	const GraphicsBuffer& buffer)
{
	return graphics_engine.get_device_module().get_buffer_device_address(buffer.get_buffer());
}

template<typename GraphicsEngineT>
uint32_t GraphicsEngineBaseModule<GraphicsEngineT>::get_num_swapchain_frames() const
{
	return graphics_engine.get_num_swapchain_images();
}
