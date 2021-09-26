#include "graphics_engine_commands.hpp"
#include "graphics_engine.hpp"

#include <mutex>


void ChangeTextureCmd::process(GraphicsEngine* engine)
{
	engine->change_texture(filename);
}

void ToggleFPSCmd::process(GraphicsEngine* engine)
{

}
