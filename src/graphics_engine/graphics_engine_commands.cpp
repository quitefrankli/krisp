#include "graphics_engine_commands.hpp"
#include "graphics_engine.hpp"
#include "objects/object.hpp"
#include "uniform_buffer_object.hpp"

#include <mutex>


void ToggleFPSCmd::process(GraphicsEngine* engine)
{

}

SpawnObjectCmd::SpawnObjectCmd(std::shared_ptr<Object> object_, uint64_t object_id)
{
	this->object_id = object_id;
	object = object_;
}

SpawnObjectCmd::SpawnObjectCmd(Object& object_, uint64_t object_id)
{
	this->object_id = object_id;
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
	};

	if (object_ref)
	{
		auto& graphics_object = engine->objects.emplace_back(
			std::make_unique<GraphicsEngineObjectRef>(*engine, *object_ref));
		spawn_object(*graphics_object);
	} else {
		auto& graphics_object = engine->objects.emplace_back(
			std::make_unique<GraphicsEngineObjectPtr>(*engine, std::move(object)));
		spawn_object(*graphics_object);
	}
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