#include "graphics_engine_object.hpp"

#include "objects.hpp"
#include "graphics_engine/graphics_engine.hpp"


GraphicsEngineObject::GraphicsEngineObject(GraphicsEngine& engine) :
	GraphicsEngineBaseModule(engine)
{
}

GraphicsEngineObject::GraphicsEngineObject(GraphicsEngine& engine, Object& object) :
	GraphicsEngineBaseModule(engine)
{
	vertex_sets = object.get_vertex_sets();
}

GraphicsEngineObject::~GraphicsEngineObject()
{
	vkDestroyBuffer(get_logical_device(), uniform_buffer, nullptr);
	vkFreeMemory(get_logical_device(), uniform_buffer_memory, nullptr);

	vkDestroyBuffer(get_logical_device(), vertex_buffer, nullptr);
	vkFreeMemory(get_logical_device(), vertex_buffer_memory, nullptr);
}
