#pragma once

#include "graphics_engine_commands.hpp"


class GraphicsEngineBase
{
public:
	virtual void handle_command(SpawnObjectCmd& cmd) {}
	virtual void handle_command(AddLightSourceCmd& cmd) {}
	virtual void handle_command(DeleteObjectCmd& cmd) {}
	virtual void handle_command(StencilObjectCmd& cmd) {}
	virtual void handle_command(UnStencilObjectCmd& cmd) {}
	virtual void handle_command(ShutdownCmd& cmd) {}
	virtual void handle_command(ToggleWireFrameModeCmd& cmd) {}
	virtual void handle_command(UpdateCommandBufferCmd& cmd) {}
	virtual void handle_command(UpdateRayTracingCmd& cmd) {}
};