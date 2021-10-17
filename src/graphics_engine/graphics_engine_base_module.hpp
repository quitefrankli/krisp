#pragma once

#include "vulkan/vulkan.hpp"


class GraphicsEngine;

class GraphicsEngineBaseModule
{
private:
	GraphicsEngine& graphics_engine;

public:
	virtual GraphicsEngine& get_graphics_engine() { return graphics_engine; }
	virtual VkDevice& get_logical_device();
	virtual VkPhysicalDevice& get_physical_device();
	virtual VkInstance& get_instance();
	virtual void create_buffer(size_t size, 
							   VkBufferUsageFlags usage_flags, 
							   VkMemoryPropertyFlags memory_flags, 
							   VkBuffer& buffer, 
							   VkDeviceMemory& device_memory);
	uint32_t get_num_swapchain_frames() const;

public:
	GraphicsEngineBaseModule() = delete;
	GraphicsEngineBaseModule(GraphicsEngine& graphics_engine);
	GraphicsEngineBaseModule(const GraphicsEngineBaseModule& module);
};