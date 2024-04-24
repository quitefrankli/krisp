#pragma once

#include <fmt/format.h>

#include "graphics_engine_object.hpp"

#include "objects/object.hpp"
#include "graphics_engine/graphics_engine.hpp"


GraphicsMesh::GraphicsMesh(Shape& shape, GraphicsEngineTexture* texture) :
	shape(shape),
	material(shape.get_material(), texture)
{
}

GraphicsEngineObject::GraphicsEngineObject(GraphicsEngine& engine, const Object& object) :
	GraphicsEngineBaseModule(engine),
	type(object.get_render_type())
{
	if (type == EPipelineType::UNASSIGNED)
	{
		throw std::runtime_error(fmt::format(
			"GraphicsEngineObject: object:={} id:={} has unassigned render type", 
			object.get_name(),
			object.get_id().get_underlying()));
	}

	if (get_render_type() == EPipelineType::CUBEMAP)
	{
		// Cubemaps are a bit special
		// we need to create a texture for each face of the cubemap
		// but currently the system only supports 1 material per shape
		// therefore only the first shape has content rest are dummys
		// that exist only to provide the texture data
		std::vector<MaterialTexture> textures;
		for (const auto& shape : object.get_shapes())
		{
			MaterialTexture texture;
			textures.push_back(shape->get_material().texture);
		}
		
		auto& texture = this->get_graphics_engine().get_texture_mgr().fetch_cubemap_texture(textures);
		meshes.emplace_back(*object.get_shapes().front(), &texture);

		return;
	}

	for (const auto& shape : object.get_shapes())
	{
		switch (get_render_type())
		{
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

GraphicsEngineObject::~GraphicsEngineObject()
{
	// this doesn't actually "deallocates" the descriptor sets, but rather makes
	// them available for reuse in the same descriptor set pool
	for (auto& mesh : meshes)
	{
		this->get_rsrc_mgr().free_dset(mesh.get_dset());
		mesh.set_dset(VK_NULL_HANDLE);
	}

	this->get_rsrc_mgr().free_dsets(dsets);
	dsets.clear();
}

uint32_t GraphicsEngineObject::get_num_unique_vertices() const
{
	return get_game_object().get_num_unique_vertices();
}

uint32_t GraphicsEngineObject::get_num_vertex_indices() const
{
	return get_game_object().get_num_vertex_indices();
}

uint32_t GraphicsEngineObject::get_num_primitives() const
{
	return get_num_vertex_indices() / 3;
}

ObjectID GraphicsEngineObject::get_id() const
{
	return get_game_object().get_id();
}

bool GraphicsEngineObject::get_visibility() const
{
	return get_game_object().get_visibility();
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