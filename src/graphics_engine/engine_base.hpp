#pragma once

#include "render_frame.hpp"
#include "recording_session.hpp"
#include "entity_component_system/material_system.hpp"
#include "entity_component_system/mesh_system.hpp"

#include <utility>
#include <stdexcept>


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
	void bind_resource_systems(MeshSystem& meshes, MaterialSystem& materials)
	{
		mesh_system = &meshes;
		material_system = &materials;
	}
	MeshSystem& get_mesh_system() const
	{
		if (!mesh_system)
			throw std::logic_error("GraphicsEngineBase: resource systems are not bound");
		return *mesh_system;
	}
	MaterialSystem& get_material_system() const
	{
		if (!material_system)
			throw std::logic_error("GraphicsEngineBase: resource systems are not bound");
		return *material_system;
	}

	// Publishing is the only scene-data path from the game thread.
	void publish_completed_render_frame(RenderFramePtr frame)
	{
		render_frame_mailbox.publish_completed(std::move(frame));
	}
	CompletedRenderFramesPtr load_latest_completed_render_frames() const
	{
		return render_frame_mailbox.load_latest();
	}
	RecordingSession& get_recording_session() { return recording_session; }
	const RecordingSession& get_recording_session() const { return recording_session; }

private:
	RenderFrameMailbox render_frame_mailbox;
	RecordingSession recording_session;
	MeshSystem* mesh_system = nullptr;
	MaterialSystem* material_system = nullptr;
};
