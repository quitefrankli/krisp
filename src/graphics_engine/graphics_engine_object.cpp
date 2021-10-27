#include "graphics_engine_object.hpp"

#include "objects.hpp"
#include "graphics_engine/graphics_engine.hpp"


GraphicsEngineObject::GraphicsEngineObject(GraphicsEngine& engine) :
	GraphicsEngineBaseModule(engine)
{
}

GraphicsEngineObject::GraphicsEngineObject(GraphicsEngine& engine, std::shared_ptr<Object>&& game_engine_object) :
	GraphicsEngineBaseModule(engine),
	object(std::move(game_engine_object))
{
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

GraphicsEngineObject::GraphicsEngineObject(GraphicsEngineObject&& graphics_object) noexcept :
	GraphicsEngineBaseModule(graphics_object.get_graphics_engine()),
	object(std::move(graphics_object.object))
{
	vertex_buffer = graphics_object.vertex_buffer;
	vertex_buffer_memory = graphics_object.vertex_buffer_memory;
	uniform_buffer = graphics_object.uniform_buffer;
	uniform_buffer_memory = graphics_object.uniform_buffer_memory;

	graphics_object.require_cleanup = false;
}

std::vector<std::vector<Vertex>>& GraphicsEngineObject::get_vertex_sets()
{
	return object->get_vertex_sets();
}
