#include "graphics_engine_commands.hpp"
#include "engine_base.hpp"


void StencilObjectCmd::process(GraphicsEngineBase* engine)
{
	engine->handle_command(*this);
}

void UnStencilObjectCmd::process(GraphicsEngineBase* engine)
{
	engine->handle_command(*this);
}

void ShutdownCmd::process(GraphicsEngineBase* engine)
{
	engine->handle_command(*this);
}

void SetRenderModeCmd::process(GraphicsEngineBase* engine)
{
	engine->handle_command(*this);
}

PreviewObjectsCmd::PreviewObjectsCmd(const std::vector<ObjectID>& objects, GuiPhotoBase& gui) :
	objects(objects),
	gui(gui)
{
}

void PreviewObjectsCmd::process(GraphicsEngineBase* engine)
{
	engine->handle_command(*this);
}
