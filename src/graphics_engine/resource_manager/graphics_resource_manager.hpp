#pragma once

#include "graphics_buffer_manager.hpp"
#include "descriptor_manager.hpp"
#include "graphics_engine/graphics_engine_swap_chain.hpp"

#include <queue>


template<typename GraphicsEngineT>
class GraphicsResourceManager : 
	virtual public GraphicsBufferManager<GraphicsEngineT>,
	virtual public GraphicsDescriptorManager<GraphicsEngineT>
{
public:
	GraphicsResourceManager(GraphicsEngineT& engine);
	virtual ~GraphicsResourceManager() override;
	VkCommandPool& get_command_pool() { return command_pool; }

	VkCommandBuffer create_command_buffer();
	
	using GraphicsBufferManager<GraphicsEngineT>::create_buffer;

private:
	void create_command_pool();

	VkCommandPool command_pool;

	using GraphicsEngineBaseModule<GraphicsEngineT>::get_graphics_engine;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_logical_device;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_rsrc_mgr;
};