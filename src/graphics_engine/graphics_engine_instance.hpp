#pragma once

#include "graphics_engine_base_module.hpp"

#include <vulkan/vulkan.hpp>

#include <string>
#include <vector>


class GraphicsEngine;

// the instance is the connection between application and Vulkan library
// its creation involves specifying some details about your application to the driver
class GraphicsEngineInstance : public GraphicsEngineBaseModule
{
public:
	GraphicsEngineInstance(GraphicsEngine& engine);
	~GraphicsEngineInstance();

	VkInstance& get() { return instance; }

private:
	const std::string APPLICATION_NAME = "My Application";
	const std::string ENGINE_NAME = "My Engine";

	std::vector<std::string> get_required_extensions() const;
	VkInstance instance;
};