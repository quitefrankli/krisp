#include "graphics_engine_commands.hpp"
#include "graphics_engine.hpp"
#include "objects/object.hpp"

#include <mutex>


void ToggleFPSCmd::process(GraphicsEngine* engine)
{

}

void SpawnObjectCmd::process(GraphicsEngine* engine)
{
	engine->spawn_object(object);
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