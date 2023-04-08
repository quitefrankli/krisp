#pragma once

#include "graphics_engine_object.hpp"

#include "objects/object.hpp"
#include "graphics_engine/graphics_engine.hpp"


template<typename GraphicsEngineT>
GraphicsEngineObject<GraphicsEngineT>::GraphicsEngineObject(GraphicsEngineT& engine, const Object& object) :
	GraphicsEngineBaseModule<GraphicsEngineT>(engine),
	type(object.get_render_type())
{
	switch (get_render_type())
	{
		case EPipelineType::STANDARD:
		case EPipelineType::CUBEMAP:
			for (const auto& shape : object.shapes)
			{
				textures.push_back(&get_graphics_engine().get_texture_mgr().create_new_unit(shape.texture));
			}
			break;
		default:
			break;
	}
}

template<typename GraphicsEngineT>
GraphicsEngineObject<GraphicsEngineT>::~GraphicsEngineObject()
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

template<typename GraphicsEngineT>
const std::vector<Shape>& GraphicsEngineObject<GraphicsEngineT>::get_shapes() const
{
	return get_game_object().shapes;
}

template<typename GraphicsEngineT>
uint32_t GraphicsEngineObject<GraphicsEngineT>::get_num_unique_vertices() const
{
	return get_game_object().get_num_unique_vertices();
}

template<typename GraphicsEngineT>
uint32_t GraphicsEngineObject<GraphicsEngineT>::get_num_vertex_indices() const
{
	return get_game_object().get_num_vertex_indices();
}

template<typename GraphicsEngineT>
inline uint32_t GraphicsEngineObject<GraphicsEngineT>::get_num_primitives() const
{
	return get_num_vertex_indices() / 3;
}

//
// Derived objects
//

template<typename GraphicsEngineT>
GraphicsEngineObjectPtr<GraphicsEngineT>::GraphicsEngineObjectPtr(GraphicsEngineT& engine, std::shared_ptr<Object>&& game_engine_object) :
	GraphicsEngineObject<GraphicsEngineT>(engine, *game_engine_object),
	object(std::move(game_engine_object))
{
}

template<typename GraphicsEngineT>
const Object& GraphicsEngineObjectPtr<GraphicsEngineT>::get_game_object() const
{
	return *object;
}

template<typename GraphicsEngineT>
GraphicsEngineObjectRef<GraphicsEngineT>::GraphicsEngineObjectRef(GraphicsEngineT& engine, Object& game_engine_object) :
	GraphicsEngineObject<GraphicsEngineT>(engine, game_engine_object),
	object(game_engine_object)
{
}

template<typename GraphicsEngineT>
const Object& GraphicsEngineObjectRef<GraphicsEngineT>::get_game_object() const
{
	return object;
}