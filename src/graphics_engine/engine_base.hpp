#pragma once

#include "graphics_engine_commands.hpp"
#include "render_frame.hpp"

#include <utility>


class EngineUiManager;
class ApplicationUiManager;

class GraphicsEngineBase
{
public:
	virtual ~GraphicsEngineBase() = default;

	virtual void handle_command(StencilObjectCmd& cmd) {}
	virtual void handle_command(UnStencilObjectCmd& cmd) {}
	virtual void handle_command(ShutdownCmd& cmd) {}
	virtual void handle_command(SetRenderModeCmd& cmd) {}
	virtual void handle_command(PreviewObjectsCmd& cmd) {}

	virtual void enqueue_cmd(std::unique_ptr<GraphicsEngineCommand>&& cmd) = 0;
	virtual float get_fps() const = 0;
	virtual EngineUiManager& get_gui_manager() = 0;
	virtual void set_application_ui_manager(ApplicationUiManager*) {}
	virtual void set_ui_layers_active(bool, bool) {}
	virtual void run() = 0;

	// Publishing is the only scene-data path from the game thread.
	void publish_completed_render_frame(RenderFramePtr frame)
	{
		render_frame_mailbox.publish_completed(std::move(frame));
	}
	CompletedRenderFramesPtr load_latest_completed_render_frames() const
	{
		return render_frame_mailbox.load_latest();
	}

private:
	RenderFrameMailbox render_frame_mailbox;
};
