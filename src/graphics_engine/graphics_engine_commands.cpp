#include "graphics_engine_commands.hpp"
#include "graphics_engine.hpp"
#include "objects/object.hpp"
#include "objects/light_source.hpp"
#include "uniform_buffer_object.hpp"

#include <mutex>


void ToggleFPSCmd::process(GraphicsEngine* engine)
{

}

SpawnObjectCmd::SpawnObjectCmd(const std::shared_ptr<Object>& object_)
{
	object = object_;
}

SpawnObjectCmd::SpawnObjectCmd(Object& object_)
{
	object_ref = &object_;
}

void SpawnObjectCmd::process(GraphicsEngine* engine)
{
	auto spawn_object = [&engine](GraphicsEngineObject& graphics_object)
	{
		engine->create_object_buffers(graphics_object);

		// uniform buffer
		engine->create_buffer(sizeof(UniformBufferObject),
					VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
					graphics_object.uniform_buffer,
					graphics_object.uniform_buffer_memory);

		const std::string& texture = graphics_object.get_game_object().texture;
		if (!texture.empty())
		{
			graphics_object.texture = &engine->texture_mgr.create_new_unit(texture);
		}
		
		engine->swap_chain.spawn_object(graphics_object);

		if (graphics_object.type == ERenderType::LIGHT_SOURCE)
		{
			engine->light_sources.emplace(
				graphics_object.get_game_object().get_id(), 
				static_cast<const LightSource&>(graphics_object.get_game_object()));
		}
	};

	if (object_ref)
	{
		auto& graphics_object = engine->objects.emplace(
			object_ref->get_id(),
			std::make_unique<GraphicsEngineObjectRef>(*engine, *object_ref));
		spawn_object(*graphics_object.first->second);
	} else {
		auto id = object->get_id();
		auto& graphics_object = engine->objects.emplace(
			id,
			std::make_unique<GraphicsEngineObjectPtr>(*engine, std::move(object)));
		spawn_object(*graphics_object.first->second);
	}
}

void DeleteObjectCmd::process(GraphicsEngine* engine)
{
	engine->get_swap_chain().get_prev_frame().mark_obj_for_delete(object_id);
	engine->get_objects()[object_id]->mark_for_delete();
}

void ShutdownCmd::process(GraphicsEngine* engine)
{
	engine->shutdown();
}

void ToggleWireFrameModeCmd::process(GraphicsEngine* engine)
{
	engine->is_wireframe_mode = !engine->is_wireframe_mode;
	engine->update_command_buffer();
}

void UpdateCommandBufferCmd::process(GraphicsEngine* engine)
{
	engine->update_command_buffer();
}