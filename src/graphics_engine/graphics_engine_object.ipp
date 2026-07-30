#pragma once

#include <fmt/format.h>

#include <algorithm>
#include <stdexcept>

#include "graphics_engine_object.hpp"

#include "graphics_engine/graphics_engine.hpp"


GraphicsEngineObject::GraphicsEngineObject(
	GraphicsEngine& engine,
	RenderObjectDefinitionPtr definition_) :
	GraphicsEngineBaseModule(engine),
	definition(std::move(definition_))
{
	if (definition->renderables.size() > CSTS::MAX_RENDERABLES_PER_OBJECT)
	{
		throw std::runtime_error(fmt::format(
			"GraphicsEngineObject: object {} has {} renderables; maximum is {}",
			definition->id.get_underlying(),
			definition->renderables.size(),
			CSTS::MAX_RENDERABLES_PER_OBJECT));
	}
}

GraphicsEngineObject::~GraphicsEngineObject()
{
	get_rsrc_mgr().free_dsets(renderable_dsets);
	for (auto& dsets : renderable_frame_dsets)
		get_rsrc_mgr().free_dsets(dsets);
}

const std::vector<RenderableDefinition>& GraphicsEngineObject::get_renderables() const
{
	return definition->renderables;
}

std::optional<SkeletonID> GraphicsEngineObject::get_skeleton_id() const
{
	const auto skeleton_id = definition->skeleton_id;
	const bool has_skinned_renderable = std::ranges::any_of(
		get_renderables(), [](const RenderableDefinition& renderable)
	{
		return is_skinned_render_type(renderable.pipeline_render_type);
	});
	if (has_skinned_renderable && !skeleton_id)
	{
		throw std::runtime_error("GraphicsEngineObject: object with skinned renderables has no skeleton");
	}
	return skeleton_id;
}

ObjectID GraphicsEngineObject::get_id() const
{
	return definition->id;
}

bool GraphicsEngineObject::get_visibility() const
{
	return get_graphics_engine()
		.get_render_object_state(get_id()).visible;
}

const glm::mat4& GraphicsEngineObject::get_model_transform() const
{
	return get_graphics_engine()
		.get_render_object_transform(get_id());
}
