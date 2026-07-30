#pragma once

#include "graphics_engine.hpp"


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

void GraphicsEngine::handle_command(SetRenderModeCmd& cmd)
{
	// Ray tracing is unsupported; retain a valid raster mode for stale callers
	// or scene data that still contains the old enum value.
	const ERenderMode requested = cmd.render_mode == ERenderMode::RAYTRACING
		? ERenderMode::RASTERIZED
		: cmd.render_mode;
	if (render_mode == requested)
		return;

	render_mode = requested;
}

void GraphicsEngine::handle_command(PreviewObjectsCmd& cmd)
{
	offscreen_rendering_objects.clear();
	for (const auto& id : cmd.objects)
	{
		auto it = objects.find(id);
		if (it == objects.end())
		{
			LOG_WARNING(Utility::get_logger(), "PreviewObjectsCmd: ignoring obj with id:={}", id.get_underlying());
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
