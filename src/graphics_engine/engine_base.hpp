#pragma once

#include "graphics_engine_commands.hpp"


class EngineUiManager;
class ApplicationUiManager;

class GraphicsEngineBase
{
public:
	virtual ~GraphicsEngineBase() = default;

	virtual void handle_command(SpawnObjectCmd& cmd) {}
	virtual void handle_command(DeleteObjectCmd& cmd) {}
	virtual void handle_command(StencilObjectCmd& cmd) {}
	virtual void handle_command(UnStencilObjectCmd& cmd) {}
	virtual void handle_command(ShutdownCmd& cmd) {}
	virtual void handle_command(SetRenderModeCmd& cmd) {}
	virtual void handle_command(UpdateCommandBufferCmd& cmd) {}
	virtual void handle_command(UpdateRayTracingCmd& cmd) {}
	virtual void handle_command(PreviewObjectsCmd& cmd) {}
	virtual void handle_command(DestroyResourcesCmd& cmd) {}
	virtual void handle_command(ResetSceneCmd& cmd) { cmd.complete.set_value(); }
	virtual void handle_command(UpdateRenderableMaterialsCmd& cmd) {}

	virtual void enqueue_cmd(std::unique_ptr<GraphicsEngineCommand>&& cmd) = 0;
	virtual float get_fps() const = 0;
	virtual uint64_t get_num_objs_deleted() const = 0;
	virtual EngineUiManager& get_gui_manager() = 0;
	virtual void set_application_ui_manager(ApplicationUiManager*) {}
	virtual void set_ui_layers_active(bool, bool) {}
	virtual void run() = 0;
	virtual void increment_num_objs_deleted() = 0;
};
