#pragma once

#include "vulkan/vulkan.hpp"


class GraphicsEngine;

class GraphicsEngineBaseModule
{
private:
	GraphicsEngine& graphics_engine;

public:
	GraphicsEngine& get_graphics_engine() { return graphics_engine; }
	VkDevice& get_logical_device();
	unsigned get_num_swap_chains();

public:
	GraphicsEngineBaseModule() = delete;
	GraphicsEngineBaseModule(GraphicsEngine& graphics_engine);
	
};