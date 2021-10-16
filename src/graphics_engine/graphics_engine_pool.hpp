#pragma once

#include "graphics_engine_base_module.hpp"


class GraphicsEnginePool : public GraphicsEngineBaseModule
{
public:
	GraphicsEnginePool(GraphicsEngine& engine);
	~GraphicsEnginePool();
	VkCommandPool& get_command_pool() { return command_pool; }

private:
	void create_command_pool();
	VkCommandPool command_pool;
};