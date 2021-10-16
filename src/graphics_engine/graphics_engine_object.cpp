#include "graphics_engine_object.hpp"

#include "graphics_engine/graphics_engine.hpp"


GraphicsEngineObject::~GraphicsEngineObject()
{
	if (!graphics_engine)
	{
		return;
	}

	vkDestroyBuffer(graphics_engine->get_logical_device(), uniform_buffer, nullptr);
	vkFreeMemory(graphics_engine->get_logical_device(), uniform_buffer_memory, nullptr);

	vkDestroyBuffer(graphics_engine->get_logical_device(), vertex_buffer, nullptr);
	vkFreeMemory(graphics_engine->get_logical_device(), vertex_buffer_memory, nullptr);
}

void GraphicsEngineObject::initialise(GraphicsEngine* graphics_engine)
{
	this->graphics_engine = graphics_engine;
}

void GraphicsEngineObject::check_initialisation()
{
	if (!graphics_engine)
	{
		throw std::runtime_error("GraphicsEngineObject: graphics_engine not initialised!");
	}
}

VkBuffer& GraphicsEngineObject::get_vertex_buffer()
{
	check_initialisation();
	return vertex_buffer;
}

VkDeviceMemory& GraphicsEngineObject::get_vertex_buffer_memory()
{
	check_initialisation();
	return vertex_buffer_memory;
}

VkBuffer& GraphicsEngineObject::get_uniform_buffer()
{
	check_initialisation();
	return uniform_buffer;
}

VkDeviceMemory& GraphicsEngineObject::get_uniform_buffer_memory()
{
	check_initialisation();
	return uniform_buffer_memory;
}
