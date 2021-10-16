#pragma once

#include <vulkan/vulkan.hpp>


class GraphicsEngine;

class GraphicsEngineObject
{
public:
	~GraphicsEngineObject();

public:
	void initialise(GraphicsEngine* graphics_engine);
	VkBuffer& get_vertex_buffer();
	VkDeviceMemory& get_vertex_buffer_memory();
	VkBuffer& get_uniform_buffer();
	VkDeviceMemory& get_uniform_buffer_memory();

private:
	void check_initialisation();

private:
	VkBuffer vertex_buffer;
	VkDeviceMemory vertex_buffer_memory;
	GraphicsEngine* graphics_engine = nullptr;

	// as opposed to vertex_buffers we expect to change uniform buffer every frame
	VkBuffer uniform_buffer;
	VkDeviceMemory uniform_buffer_memory;
};