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

GraphicsEngineObject::GraphicsEngineObject(GraphicsEngineObject&& graphics_object) noexcept :
	GraphicsEngineBaseModule(graphics_object.get_graphics_engine()),
	object(std::move(graphics_object.object))
{
	vertex_buffer = graphics_object.vertex_buffer;
	vertex_buffer_memory = graphics_object.vertex_buffer_memory;
	index_buffer = graphics_object.index_buffer;
	index_buffer_memory = graphics_object.index_buffer_memory;
	uniform_buffer = graphics_object.uniform_buffer;
	uniform_buffer_memory = graphics_object.uniform_buffer_memory;
	texture = graphics_object.texture;

	graphics_object.require_cleanup = false;
}

GraphicsEngineObject::~GraphicsEngineObject()
{
	if (!require_cleanup)
	{
		return;
	}

	vkDestroyBuffer(get_logical_device(), vertex_buffer, nullptr);
	vkFreeMemory(get_logical_device(), vertex_buffer_memory, nullptr);

	vkDestroyBuffer(get_logical_device(), index_buffer, nullptr);
	vkFreeMemory(get_logical_device(), index_buffer_memory, nullptr);

	vkDestroyBuffer(get_logical_device(), uniform_buffer, nullptr);
	vkFreeMemory(get_logical_device(), uniform_buffer_memory, nullptr);
}

const std::vector<Shape>& GraphicsEngineObject::get_shapes() const
{
	return object->shapes;
}

uint32_t GraphicsEngineObject::get_num_unique_vertices() const
{
	return object->get_num_unique_vertices();
}

uint32_t GraphicsEngineObject::get_num_vertex_indices() const
{
	return object->get_num_vertex_indices();
}

VkImageView& GraphicsEngineObject::get_texture_image_view() 
{
	return texture->get_texture_image_view();
}

VkSampler& GraphicsEngineObject::get_texture_sampler() 
{ 
	return texture->get_texture_sampler();
}