#pragma once

#include "graphics_engine_base_module.hpp"
#include "objects.hpp"

#include <vulkan/vulkan.hpp>


class GraphicsEngine;
class Object;

class GraphicsEngineObject : public GraphicsEngineBaseModule, public ObjectAbstract
{
public:
	GraphicsEngineObject(GraphicsEngine& engine);
	GraphicsEngineObject(GraphicsEngine& engine, Object& object);
	GraphicsEngineObject(GraphicsEngineObject&& object) noexcept;

	~GraphicsEngineObject();

	std::vector<std::vector<Vertex>> vertex_sets;

	VkBuffer vertex_buffer;
	VkDeviceMemory vertex_buffer_memory;

	// as opposed to vertex_buffers we expect to change uniform buffer every frame
	VkBuffer uniform_buffer;
	VkDeviceMemory uniform_buffer_memory;
	bool require_cleanup = true;
};