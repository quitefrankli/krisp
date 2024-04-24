#pragma once

#include "graphics_engine_commands.hpp"


class GuiManager;

class GraphicsEngineBase
{
public:
	virtual void handle_command(SpawnObjectCmd& cmd) {}
	virtual void handle_command(DeleteObjectCmd& cmd) {}
	virtual void handle_command(StencilObjectCmd& cmd) {}
	virtual void handle_command(UnStencilObjectCmd& cmd) {}
	virtual void handle_command(ShutdownCmd& cmd) {}
	virtual void handle_command(ToggleWireFrameModeCmd& cmd) {}
	virtual void handle_command(UpdateCommandBufferCmd& cmd) {}
	virtual void handle_command(UpdateRayTracingCmd& cmd) {}
	virtual void handle_command(PreviewObjectsCmd& cmd) {}

	virtual void enqueue_cmd(std::unique_ptr<GraphicsEngineCommand>&& cmd) = 0;
	virtual float get_fps() const = 0;
	virtual uint64_t get_num_objs_deleted() const = 0;
	virtual GuiManager& get_gui_manager() = 0;
	virtual void run() = 0;
	virtual void increment_num_objs_deleted() = 0;
};