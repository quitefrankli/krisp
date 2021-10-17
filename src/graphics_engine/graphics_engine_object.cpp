#include "graphics_engine_object.hpp"

#include "objects.hpp"
#include "graphics_engine/graphics_engine.hpp"


GraphicsEngineObject::GraphicsEngineObject(GraphicsEngine& engine) :
	GraphicsEngineBaseModule(engine)
{
}

GraphicsEngineObject::GraphicsEngineObject(GraphicsEngine& engine, Object& object) :
	GraphicsEngineBaseModule(engine), ObjectAbstract(object.get_id())
{
	vertex_sets = object.get_vertex_sets();
}

GraphicsEngineObject::~GraphicsEngineObject()
{
	if (!require_cleanup)
	{
		return;
	}

	vkDestroyBuffer(get_logical_device(), uniform_buffer, nullptr);
	vkFreeMemory(get_logical_device(), uniform_buffer_memory, nullptr);

	vkDestroyBuffer(get_logical_device(), vertex_buffer, nullptr);
	vkFreeMemory(get_logical_device(), vertex_buffer_memory, nullptr);
}

GraphicsEngineObject::GraphicsEngineObject(GraphicsEngineObject&& object) noexcept :
	GraphicsEngineBaseModule(std::move(object)),
	vertex_sets(std::move(object.vertex_sets))
{
	vertex_buffer = object.vertex_buffer;
	vertex_buffer_memory = object.vertex_buffer_memory;
	uniform_buffer = object.uniform_buffer;
	uniform_buffer_memory = object.uniform_buffer_memory;

	object.require_cleanup = false;
}
