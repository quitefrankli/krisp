#pragma once

#include "vulkan/vulkan.hpp"

#define LOAD_VK_FUNCTION(name) reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(get_logical_device(), #name))


template<typename GraphicsEngineT>
class GraphicsEngineBaseModule
{
public:
	GraphicsEngineBaseModule() = delete;
	GraphicsEngineBaseModule(const GraphicsEngineBaseModule&) = delete;
	GraphicsEngineBaseModule& operator=(const GraphicsEngineBaseModule&) = delete;

	GraphicsEngineBaseModule(GraphicsEngineT& graphics_engine);
	GraphicsEngineBaseModule(GraphicsEngineBaseModule&& base_module) noexcept = default;

	virtual ~GraphicsEngineBaseModule() = default;

public:
	virtual GraphicsEngineT& get_graphics_engine() { return graphics_engine; }
	virtual VkDevice& get_logical_device();
	virtual VkPhysicalDevice& get_physical_device();
	virtual VkInstance& get_instance();
	virtual void create_buffer(size_t size, 
							   VkBufferUsageFlags usage_flags, 
							   VkMemoryPropertyFlags memory_flags, 
							   VkBuffer& buffer, 
							   VkDeviceMemory& device_memory);
	uint32_t get_num_swapchain_frames() const;

protected:
	// for derived classes that we may not want to call destructor on because of a std::move
	bool should_destroy = true;

private:
	GraphicsEngineT& graphics_engine;
};