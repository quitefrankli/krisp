#pragma once

#include "graphics_buffer_manager.hpp"
#include "descriptor_manager.hpp"
#include "graphics_engine/graphics_engine_swap_chain.hpp"

#include <queue>


class GraphicsResourceManager : 
	public GraphicsBufferManager,
	public GraphicsDescriptorManager
{
public:
	GraphicsResourceManager(GraphicsEngine& engine);
	virtual ~GraphicsResourceManager() override;
	VkCommandPool& get_command_pool() { return command_pool; }

	VkCommandBuffer create_command_buffer();

private:
	void create_command_pool();

	VkCommandPool command_pool;
};