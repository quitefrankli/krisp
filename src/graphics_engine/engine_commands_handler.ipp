#pragma once

#include "graphics_engine.hpp"
#include "objects/light_source.hpp"
#include "shared_data_structures.hpp"
#include "renderers/renderers.hpp"
#include "resource_manager/buffer_map.hpp"


template<typename GameEngineT>
void GraphicsEngine<GameEngineT>::handle_command(SpawnObjectCmd& cmd)
{
	auto spawn_object = [&](GraphicsEngineObject<GraphicsEngine>& graphics_object)
	{
		// vertex buffer doesn't change per frame so unlike uniform buffer it doesn't need to be 
		// per frame resource and therefore we only need 1 copy
		const auto id = graphics_object.get_game_object().get_id();
		get_rsrc_mgr().reserve_vertex_buffer(id, graphics_object.get_game_object().get_vertices_data_size());
		get_rsrc_mgr().reserve_index_buffer(id, graphics_object.get_game_object().get_indices_data_size());
		get_rsrc_mgr().write_shapes_to_buffers(graphics_object.get_shapes(), id);

		// upload materials
		for (int i = 0; i < graphics_object.get_shapes().size(); ++i)
		{
			const ShapePtr& shape = graphics_object.get_shapes()[i];
			const SDS::MaterialData& material = graphics_object.get_materials()[i].get_data();
			get_rsrc_mgr().reserve_materials_buffer(shape->get_id(), sizeof(material));
			get_rsrc_mgr().write_to_materials_buffer(material, shape->get_id());
		}

		// TODO: we need per frame uniform buffers, could alternatively use a single buffer
		get_rsrc_mgr().reserve_uniform_buffer(id, sizeof(SDS::ObjectData));

		BufferMapEntry buffer_map;
		buffer_map.vertex_offset = get_rsrc_mgr().get_vertex_buffer_offset(id);
		buffer_map.index_offset = get_rsrc_mgr().get_index_buffer_offset(id);
		buffer_map.uniform_offset = get_rsrc_mgr().get_uniform_buffer_offset(id);
		get_rsrc_mgr().write_to_mapping_buffer(buffer_map, id);

		swap_chain.spawn_object(graphics_object);
	};

	if (cmd.object_ref)
	{
		auto graphics_object = objects.emplace(
			cmd.object_ref->get_id(),
			std::make_unique<GraphicsEngineObjectRef<GraphicsEngine>>(*this, *cmd.object_ref));
		spawn_object(*graphics_object.first->second);
	} else {
		auto id = cmd.object->get_id();
		auto graphics_object = objects.emplace(
			id,
			std::make_unique<GraphicsEngineObjectPtr<GraphicsEngine>>(*this, std::move(cmd.object)));
		spawn_object(*graphics_object.first->second);
	}
}

template<typename GameEngineT>
void GraphicsEngine<GameEngineT>::handle_command(AddLightSourceCmd& cmd)
{
	light_sources.emplace(cmd.object.get_id(), cmd.object);
}

template<typename GameEngineT>
void GraphicsEngine<GameEngineT>::handle_command(DeleteObjectCmd& cmd)
{
	get_swap_chain().get_prev_frame().mark_obj_for_delete(cmd.object_id);
	get_objects()[cmd.object_id]->mark_for_delete();
}

template<typename GameEngineT>
void GraphicsEngine<GameEngineT>::handle_command(StencilObjectCmd& cmd)
{
	stenciled_objects.insert(cmd.object_id);
}

template<typename GameEngineT>
void GraphicsEngine<GameEngineT>::handle_command(UnStencilObjectCmd& cmd)
{
	stenciled_objects.erase(cmd.object_id);
}

template<typename GameEngineT>
void GraphicsEngine<GameEngineT>::handle_command(ShutdownCmd& cmd)
{
	shutdown();
}

template<typename GameEngineT>
void GraphicsEngine<GameEngineT>::handle_command(ToggleWireFrameModeCmd& cmd)
{
    is_wireframe_mode = !is_wireframe_mode;
	update_command_buffer();
}

template<typename GameEngineT>
void GraphicsEngine<GameEngineT>::handle_command(UpdateCommandBufferCmd& cmd)
{
	update_command_buffer();
}

template<typename GameEngineT>
void GraphicsEngine<GameEngineT>::handle_command(UpdateRayTracingCmd& cmd)
{
	raytracing_component.update_acceleration_structures();
	static_cast<RaytracingRenderer<GraphicsEngine>&>(
		renderer_mgr.get_renderer(ERendererType::RAYTRACING)).update_rt_dsets();
}

template<typename GameEngineT>
void GraphicsEngine<GameEngineT>::handle_command(PreviewObjectsCmd& cmd)
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

	Renderer<GraphicsEngine>& renderer = get_renderer_mgr().get_renderer(ERendererType::OFFSCREEN_GUI_VIEWPORT);
	const auto extent = renderer.get_extent();
	get_graphics_gui_manager().update_preview_window(
		cmd.gui, 
		get_texture_mgr().fetch_sampler(ETextureSamplerType::ADDR_MODE_CLAMP_TO_EDGE),
		renderer.get_output_image_view(),
		glm::uvec2(extent.width, extent.height));
}
