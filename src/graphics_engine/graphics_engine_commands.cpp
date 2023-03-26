#include "graphics_engine_commands.hpp"
#include "engine_base.hpp"
#include "objects/object.hpp"


SpawnObjectCmd::SpawnObjectCmd(const std::shared_ptr<Object>& object_)
{
	object = object_;
}

SpawnObjectCmd::SpawnObjectCmd(Object& object_)
{
	object_ref = &object_;
}

ObjectCommand::ObjectCommand(const Object& object) : object_id(object.get_id())
{
}

void SpawnObjectCmd::process(GraphicsEngineBase* engine)
{
	engine->handle_command(*this);
}

void DeleteObjectCmd::process(GraphicsEngineBase* engine)
{
	engine->handle_command(*this);
}

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

void ToggleWireFrameModeCmd::process(GraphicsEngineBase* engine)
{
	engine->handle_command(*this);
}

void UpdateCommandBufferCmd::process(GraphicsEngineBase* engine)
{
	engine->handle_command(*this);
}

void UpdateRayTracingCmd::process(GraphicsEngineBase* engine) {
	engine->handle_command(*this);
}
