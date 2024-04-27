#pragma once

#include <fmt/format.h>

#include "graphics_engine_object.hpp"

#include "objects/object.hpp"
#include "graphics_engine/graphics_engine.hpp"


GraphicsEngineObject::GraphicsEngineObject(GraphicsEngine& engine, const Object& object) :
	GraphicsEngineBaseModule(engine),
	per_frame_object_dsets(CSTS::NUM_EXPECTED_SWAPCHAIN_IMAGES, nullptr)
{
	// TODO: need to fixup cubemaps
	// if (get_render_type() == EPipelineType::CUBEMAP)
	// {
	// 	// Cubemaps are a bit special
	// 	// we need to create a texture for each face of the cubemap
	// 	// but currently the system only supports 1 material per shape
	// 	// therefore only the first shape has content rest are dummys
	// 	// that exist only to provide the texture data
	// 	std::vector<MaterialTexture> textures;
	// 	for (const auto& shape : object.get_shapes())
	// 	{
	// 		MaterialTexture texture;
	// 		textures.push_back(shape->get_material().texture);
	// 	}
		
	// 	auto& texture = this->get_graphics_engine().get_texture_mgr().fetch_cubemap_texture(textures);
	// 	meshes.emplace_back(*object.get_shapes().front(), &texture);

	// 	return;
	// }


	for (const auto& renderable : object.renderables)
	{
		switch (renderable.pipeline_render_type)
		{
			// TODO: need to implement this for textures
			// case EPipelineType::SKINNED:
			// case EPipelineType::STANDARD:
			// 	meshes.emplace_back(*shape, &this->get_graphics_engine().get_texture_mgr().fetch_texture(
			// 		shape->get_material().texture, ETextureSamplerType::ADDR_MODE_REPEAT));
			// 	break;
			default:
				break;
		}
	}
}

GraphicsEngineObject::~GraphicsEngineObject()
{
	get_rsrc_mgr().free_dsets(renderable_dsets);
	get_rsrc_mgr().free_dsets(per_frame_object_dsets);
}

const std::vector<Renderable>& GraphicsEngineObject::get_renderables() const
{
	return get_game_object().renderables;
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