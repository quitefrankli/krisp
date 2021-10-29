#include "graphics_engine_commands.hpp"
#include "graphics_engine.hpp"
#include "objects.hpp"

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