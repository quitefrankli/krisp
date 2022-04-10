#include "graphics_engine_object.hpp"

#include "objects/object.hpp"
#include "graphics_engine/graphics_engine.hpp"


GraphicsEngineObject::GraphicsEngineObject(GraphicsEngine& engine, const Object& object) :
	GraphicsEngineBaseModule(engine),
	type(object.get_render_type())
{
	switch (get_render_type())
	{
		case ERenderType::STANDARD:
		case ERenderType::CUBEMAP:
			for (const auto& shape : object.shapes)
			{
				textures.push_back(&get_graphics_engine().get_texture_mgr().create_new_unit(shape.texture));
			}
			break;
		default:
			break;
	}
}

GraphicsEngineObject::~GraphicsEngineObject()
{
	vkDestroyBuffer(get_logical_device(), vertex_buffer, nullptr);
	vkFreeMemory(get_logical_device(), vertex_buffer_memory, nullptr);

	vkDestroyBuffer(get_logical_device(), index_buffer, nullptr);
	vkFreeMemory(get_logical_device(), index_buffer_memory, nullptr);

	vkDestroyBuffer(get_logical_device(), uniform_buffer, nullptr);
	vkFreeMemory(get_logical_device(), uniform_buffer_memory, nullptr);

	// this doesn't actually "deallocates" the descriptor sets, but rather makes
	// them available for reuse in the same descriptor set pool
	get_graphics_engine().get_graphics_resource_manager().free_descriptor_sets(descriptor_sets);
}

const std::vector<Shape>& GraphicsEngineObject::get_shapes() const
{
	return get_game_object().shapes;
}

uint32_t GraphicsEngineObject::get_num_unique_vertices() const
{
	return get_game_object().get_num_unique_vertices();
}

uint32_t GraphicsEngineObject::get_num_vertex_indices() const
{
	return get_game_object().get_num_vertex_indices();
}

//
// Derived objects
//

GraphicsEngineObjectPtr::GraphicsEngineObjectPtr(GraphicsEngine& engine, std::shared_ptr<Object>&& game_engine_object) :
	GraphicsEngineObject(engine, *game_engine_object),
	object(std::move(game_engine_object))
{
}

const Object& GraphicsEngineObjectPtr::get_game_object() const
{
	return *object;
}

GraphicsEngineObjectRef::GraphicsEngineObjectRef(GraphicsEngine& engine, Object& game_engine_object) :
	GraphicsEngineObject(engine, game_engine_object),
	object(game_engine_object)
{
}

const Object& GraphicsEngineObjectRef::get_game_object() const
{
	return object;
}