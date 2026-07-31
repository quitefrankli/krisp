#pragma once

#include "graphics_renderable.hpp"

#include "graphics_engine/graphics_engine.hpp"

#include <stdexcept>


GraphicsRenderable::GraphicsRenderable(
	GraphicsEngine& engine,
	RenderableDefinitionPtr definition_) :
	GraphicsEngineBaseModule(engine),
	definition(std::move(definition_)),
	frame_allocation_count(0)
{
	if (!definition)
		throw std::invalid_argument("GraphicsRenderable: definition is empty");
	(void)get_skeleton_id();
}

GraphicsRenderable::~GraphicsRenderable()
{
	for (uint32_t frame_index = 0;
		frame_index < frame_allocation_count; ++frame_index)
	{
		get_rsrc_mgr().free_buffer(
			RenderableFrameID{get_id(), frame_index});
	}
	if (dset != VK_NULL_HANDLE)
	{
		std::vector<VkDescriptorSet> dsets{ dset };
		get_rsrc_mgr().free_dsets(dsets);
	}
	get_rsrc_mgr().free_dsets(frame_dsets);
}

GraphicsRenderableResources GraphicsRenderable::take_graphics_resources() noexcept
{
	GraphicsRenderableResources resources{
		get_id(), frame_allocation_count, dset, std::move(frame_dsets)};
	frame_allocation_count = 0;
	dset = VK_NULL_HANDLE;
	frame_dsets.clear();
	return resources;
}

std::optional<SkeletonID> GraphicsRenderable::get_skeleton_id() const
{
	const bool skinned = is_skinned_render_type(definition->pipeline_render_type);
	if (skinned != definition->skeleton_id.has_value())
		throw std::runtime_error(
			"GraphicsRenderable: skeleton binding does not match render type");
	return definition->skeleton_id;
}

bool GraphicsRenderable::get_visibility() const
{
	return get_graphics_engine().get_renderable_state(get_id()).visible;
}

const glm::mat4& GraphicsRenderable::get_model_transform() const
{
	return get_graphics_engine().get_renderable_state(get_id()).model_transform;
}
