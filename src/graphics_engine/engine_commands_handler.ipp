#pragma once

#include "graphics_engine.hpp"
#include "objects/light_source.hpp"
#include "uniform_buffer_object.hpp"
#include "renderers/renderers.hpp"


template<typename GameEngineT>
void GraphicsEngine<GameEngineT>::handle_command(SpawnObjectCmd& cmd)
{
	auto spawn_object = [&](GraphicsEngineObject<GraphicsEngine>& graphics_object)
	{
		create_object_buffers(graphics_object);

		// uniform buffer
		create_buffer(sizeof(UniformBufferObject),
					VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
					graphics_object.uniform_buffer,
					graphics_object.uniform_buffer_memory);

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
	ray_tracing.update_acceleration_structures();
	static_cast<RaytracingRenderer<GraphicsEngine>&>(
		renderer_mgr.get_renderer(ERendererType::RAYTRACING)).update_rt_dsets();
}
