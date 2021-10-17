#pragma once

#include "graphics_engine_base_module.hpp"


class GraphicsEnginePool : public GraphicsEngineBaseModule
{
public:
	GraphicsEnginePool(GraphicsEngine& engine);
	~GraphicsEnginePool();
	VkCommandPool& get_command_pool() { return command_pool; }
	VkDescriptorPool descriptor_pool;
	VkDescriptorSetLayout descriptor_set_layout;

private:
	void create_command_pool();
	void create_descriptor_pool();
	void create_descriptor_set_layout();
	VkCommandPool command_pool;
};