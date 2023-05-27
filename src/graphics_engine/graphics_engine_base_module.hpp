#pragma once

#include "vulkan/vulkan.hpp"

#include "resource_manager/graphics_buffer.hpp"


#define LOAD_VK_FUNCTION(name) reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(this->get_logical_device(), #name))

template<typename GraphicsEngineT>
class GraphicsResourceManager;

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
	GraphicsResourceManager<GraphicsEngineT>& get_rsrc_mgr();
	const GraphicsResourceManager<GraphicsEngineT>& get_rsrc_mgr() const;
	virtual GraphicsBuffer create_buffer(
		size_t size, 
		VkBufferUsageFlags usage_flags, 
		VkMemoryPropertyFlags memory_flags);
	virtual VkDeviceAddress get_buffer_device_address(const GraphicsBuffer& buffer);		
	uint32_t get_num_swapchain_frames() const;

protected:
	// for derived classes that we may not want to call destructor on because of a std::move
	bool should_destroy = true;

private:
	GraphicsEngineT& graphics_engine;
};