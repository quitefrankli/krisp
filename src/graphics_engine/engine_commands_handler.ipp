#pragma once

#include "graphics_engine.hpp"
#include "objects/light_source.hpp"
#include "uniform_buffer_object.hpp"
#include "renderers/renderers.hpp"
#include "resource_manager/buffer_map.hpp"


template<typename GameEngineT>
void GraphicsEngine<GameEngineT>::handle_command(SpawnObjectCmd& cmd)
{
	auto spawn_object = [&](GraphicsEngineObject<GraphicsEngine>& graphics_object)
	{
		// vertex buffer doesn't change per frame so unlike uniform buffer it doesn't need to be 
		// per frame resource and therefore we only need 1 copy
		const uint32_t id = graphics_object.get_game_object().get_id();
		pool.reserve_vertex_buffer(id, graphics_object.get_num_unique_vertices() * sizeof(Vertex));
		pool.reserve_index_buffer(id, graphics_object.get_num_vertex_indices() * sizeof(uint32_t));
		pool.write_shapes_to_buffers(graphics_object.get_shapes(), id);

		// TODO: we need per frame uniform buffers, could alternatively use a single buffer
		get_graphics_resource_manager().reserve_uniform_buffer(id, sizeof(UniformBufferObject));

		BufferMapEntry buffer_map;
		buffer_map.vertex_offset = pool.get_vertex_buffer_offset(id);
		buffer_map.index_offset = pool.get_index_buffer_offset(id);
		buffer_map.uniform_offset = pool.get_uniform_buffer_offset(id);
		pool.write_to_mapping_buffer(buffer_map, id);

		swap_chain.spawn_object(graphics_object);

		if (graphics_object.type == EPipelineType::LIGHT_SOURCE)
		{
			light_sources.emplace(
				graphics_object.get_game_object().get_id(), 
				static_cast<const LightSource&>(graphics_object.get_game_object()));
		}
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
inline void GraphicsEngine<GameEngineT>::handle_command(UpdateRayTracingCmd& cmd)
{
	raytracing_component.update_acceleration_structures();
	static_cast<RaytracingRenderer<GraphicsEngine>&>(
		renderer_mgr.get_renderer(ERendererType::RAYTRACING)).update_rt_dsets();
}
