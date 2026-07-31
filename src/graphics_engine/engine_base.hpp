#pragma once

#include "render_frame.hpp"

#include <utility>


class EngineUiManager;
class ApplicationUiManager;

class GraphicsEngineBase
{
public:
	virtual ~GraphicsEngineBase() = default;

	virtual void request_shutdown() = 0;
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
