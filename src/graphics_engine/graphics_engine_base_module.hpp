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
	virtual unsigned get_num_swap_chains() const;
	virtual VkInstance& get_instance();

public:
	GraphicsEngineBaseModule() = delete;
	GraphicsEngineBaseModule(GraphicsEngine& graphics_engine);
};