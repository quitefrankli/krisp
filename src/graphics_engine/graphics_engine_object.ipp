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
			for (const auto& shape : object.get_shapes())
			{
				materials.emplace_back(
					shape->get_material(), 
					&get_graphics_engine().get_texture_mgr().fetch_texture(
					shape->get_material().texture, ETextureSamplerType::ADDR_MODE_REPEAT));
			}
		case EPipelineType::CUBEMAP:
			for (const auto& shape : object.get_shapes())
			{
				materials.emplace_back(
					shape->get_material(), 
					&get_graphics_engine().get_texture_mgr().fetch_texture(
					shape->get_material().texture, ETextureSamplerType::ADDR_MODE_CLAMP_TO_EDGE));
			}
			break;
		default:
			for (const auto& shape : object.get_shapes())
			{
				materials.emplace_back(shape->get_material());
			}
			break;
	}
}

template<typename GraphicsEngineT>
GraphicsEngineObject<GraphicsEngineT>::~GraphicsEngineObject()
{
	// this doesn't actually "deallocates" the descriptor sets, but rather makes
	// them available for reuse in the same descriptor set pool
	get_rsrc_mgr().free_high_frequency_dsets(descriptor_sets);
}

template<typename GraphicsEngineT>
const std::vector<ShapePtr>& GraphicsEngineObject<GraphicsEngineT>::get_shapes() const
{
	return get_game_object().get_shapes();
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