#pragma once

#include <fmt/format.h>

#include "graphics_engine_object.hpp"

#include "objects/object.hpp"
#include "graphics_engine/graphics_engine.hpp"


GraphicsEngineObject::GraphicsEngineObject(GraphicsEngine& engine, const Object& object) :
	GraphicsEngineBaseModule(engine),
	per_frame_object_dsets(CSTS::NUM_EXPECTED_SWAPCHAIN_IMAGES, nullptr)
{
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