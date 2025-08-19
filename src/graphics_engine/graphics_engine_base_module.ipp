#pragma once

#include "graphics_engine_base_module.hpp"
#include "graphics_engine/graphics_engine.hpp"


GraphicsEngineBaseModule::GraphicsEngineBaseModule(GraphicsEngine& engine) :
	graphics_engine(engine)
{
}

VkDevice& GraphicsEngineBaseModule::get_logical_device()
{
	return graphics_engine.get_logical_device();
}

VkPhysicalDevice& GraphicsEngineBaseModule::get_physical_device()
{
	return graphics_engine.get_physical_device();
}

VkInstance& GraphicsEngineBaseModule::get_instance()
{
	return graphics_engine.get_instance();
}

GraphicsResourceManager& GraphicsEngineBaseModule::get_rsrc_mgr()
{
	return graphics_engine.get_rsrc_mgr();
}

const GraphicsResourceManager& GraphicsEngineBaseModule::get_rsrc_mgr() const
{
	return graphics_engine.get_rsrc_mgr();
}

GraphicsBuffer GraphicsEngineBaseModule::create_buffer(
	size_t size, 
	VkBufferUsageFlags usage_flags, 
	VkMemoryPropertyFlags memory_flags,
	uint32_t alignment)
{
	return static_cast<GraphicsBufferManager&>(get_rsrc_mgr()).create_buffer(size, usage_flags, memory_flags, alignment);
}

VkDeviceAddress GraphicsEngineBaseModule::get_buffer_device_address(
	const GraphicsBuffer& buffer)
{
	return graphics_engine.get_device_module().get_buffer_device_address(buffer.get_buffer());
}
