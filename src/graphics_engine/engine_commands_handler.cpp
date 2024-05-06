#pragma once

#include "graphics_engine.hpp"
#include "shared_data_structures.hpp"
#include "renderers/renderers.hpp"
#include "shared_data_structures.hpp"
#include "entity_component_system/ecs.hpp"


void GraphicsEngine::handle_command(SpawnObjectCmd& cmd)
{
	const auto spawn_object = [&](GraphicsEngineObject& graphics_object)
	{
		spawn_object_create_buffers(graphics_object);
		spawn_object_create_dsets(graphics_object);
	};

	// TODO: delete me
	const std::unordered_set<EPipelineType> supported_render_types = 
	{
		EPipelineType::COLOR,
		EPipelineType::CUBEMAP
	};

	if (cmd.object_ref)
	{
		// TODO: delete me
		for (const auto& renderable : cmd.object_ref->renderables)
		{
			if (!supported_render_types.contains(renderable.pipeline_render_type))
			{
				return;
			}
		}
		auto graphics_object = objects.emplace(
			cmd.object_ref->get_id(),
			std::make_unique<GraphicsEngineObjectRef>(*this, *cmd.object_ref));
		spawn_object(*graphics_object.first->second);
	} else {
		// TODO: delete me
		for (const auto& renderable : cmd.object->renderables)
		{
			if (!supported_render_types.contains(renderable.pipeline_render_type))
			{
				return;
			}
		}
		auto id = cmd.object->get_id();
		auto graphics_object = objects.emplace(
			id,
			std::make_unique<GraphicsEngineObjectPtr>(*this, std::move(cmd.object)));
		spawn_object(*graphics_object.first->second);
	}
}

void GraphicsEngine::handle_command(DeleteObjectCmd& cmd)
{
	get_swap_chain().get_prev_frame().mark_obj_for_delete(cmd.object_id);
	get_objects()[cmd.object_id]->mark_for_delete();
}

void GraphicsEngine::handle_command(StencilObjectCmd& cmd)
{
	stenciled_objects.insert(cmd.object_id);
}

void GraphicsEngine::handle_command(UnStencilObjectCmd& cmd)
{
	stenciled_objects.erase(cmd.object_id);
}

void GraphicsEngine::handle_command(ShutdownCmd& cmd)
{
	shutdown();
}

void GraphicsEngine::handle_command(ToggleWireFrameModeCmd& cmd)
{
    is_wireframe_mode = !is_wireframe_mode;
	update_command_buffer();
}

void GraphicsEngine::handle_command(UpdateCommandBufferCmd& cmd)
{
	update_command_buffer();
}

void GraphicsEngine::handle_command(UpdateRayTracingCmd& cmd)
{
	raytracing_component.update_acceleration_structures();
	static_cast<RaytracingRenderer&>(
		renderer_mgr.get_renderer(ERendererType::RAYTRACING)).update_rt_dsets();
}

void GraphicsEngine::handle_command(PreviewObjectsCmd& cmd)
{
	offscreen_rendering_objects.clear();
	for (Object* object : cmd.objects)
	{
		const auto id = object->get_id();
		auto it = objects.find(id);
		if (it == objects.end())
		{
			LOG_WARNING(Utility::get().get_logger(), "PreviewObjectsCmd: ignoring obj with id:={}", id.get_underlying());
			continue;
		}

		offscreen_rendering_objects.emplace(id, it->second.get());
	}

	Renderer& renderer = get_renderer_mgr().get_renderer(ERendererType::OFFSCREEN_GUI_VIEWPORT);
	const auto extent = renderer.get_extent();
	get_graphics_gui_manager().update_preview_window(
		cmd.gui, 
		get_texture_mgr().fetch_sampler(ETextureSamplerType::ADDR_MODE_CLAMP_TO_EDGE),
		renderer.get_output_image_view(get_swap_chain().get_curr_frame().image_index),
		glm::uvec2(extent.width, extent.height));
}
