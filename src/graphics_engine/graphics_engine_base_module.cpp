#include "graphics_engine_base_module.hpp"

#include "graphics_engine.hpp"


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

void GraphicsEngineBaseModule::create_buffer(size_t size, 
											 VkBufferUsageFlags usage_flags, 
											 VkMemoryPropertyFlags memory_flags, 
											 VkBuffer& buffer, 
											 VkDeviceMemory& device_memory)
{
	graphics_engine.create_buffer(size, usage_flags, memory_flags, buffer, device_memory);
}

uint32_t GraphicsEngineBaseModule::get_num_swapchain_frames() const
{
	return graphics_engine.get_num_swapchain_images();
}