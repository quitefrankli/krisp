#pragma once

#include "graphics_engine_object.hpp"

#include "objects/object.hpp"
#include "graphics_engine/graphics_engine.hpp"


GraphicsMesh::GraphicsMesh(Shape& shape, GraphicsEngineTexture* texture) :
	shape(shape),
	material(shape.get_material(), texture)
{
}

template<typename GraphicsEngineT>
GraphicsEngineObject<GraphicsEngineT>::GraphicsEngineObject(GraphicsEngineT& engine, const Object& object) :
	GraphicsEngineBaseModule<GraphicsEngineT>(engine),
	type(object.get_render_type())
{
	for (const auto& shape : object.get_shapes())
	{
		switch (get_render_type())
		{
			case EPipelineType::CUBEMAP:
				meshes.emplace_back(*shape, &this->get_graphics_engine().get_texture_mgr().fetch_texture(
					shape->get_material().texture, ETextureSamplerType::ADDR_MODE_CLAMP_TO_EDGE));
				break;
			case EPipelineType::SKINNED:
			case EPipelineType::STANDARD:
				meshes.emplace_back(*shape, &this->get_graphics_engine().get_texture_mgr().fetch_texture(
					shape->get_material().texture, ETextureSamplerType::ADDR_MODE_REPEAT));
				break;
			default:
				meshes.emplace_back(*shape);
				break;
		}
	}
}

template<typename GraphicsEngineT>
GraphicsEngineObject<GraphicsEngineT>::~GraphicsEngineObject()
{
	// this doesn't actually "deallocates" the descriptor sets, but rather makes
	// them available for reuse in the same descriptor set pool
	for (auto& mesh : meshes)
	{
		this->get_rsrc_mgr().free_dsets(mesh.get_dsets());
		mesh.get_dsets().clear();
	}

	this->get_rsrc_mgr().free_dsets(dsets);
	dsets.clear();
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