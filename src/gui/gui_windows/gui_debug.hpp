#pragma once

#include "gui_windows.hpp"

#include <unordered_map>

class GuiDebug : public EngineUiWindow
{
public:
	GuiDebug();
	void process(GameEngine& engine) override;
	void draw() override;
	bool consume_screenshot_request()
	{
		const bool request = should_take_screenshot;
		should_take_screenshot = false;
		return request;
	}

	bool consume_start_recording_request()
	{
		const bool request = should_start_recording;
		should_start_recording = false;
		return request;
	}

	bool consume_stop_recording_request()
	{
		const bool request = should_stop_recording;
		should_stop_recording = false;
		return request;
	}

	void set_is_recording(bool value) { is_recording = value; }

private:
	void sync_collider_visualisers(GameEngine& engine);
	void clear_collider_visualisers(GameEngine& engine);

	bool should_refresh_objects_list = false;
	bool should_toggle_pause = false;
	bool should_take_screenshot = false;
	bool should_start_recording = false;
	bool should_stop_recording = false;
	bool is_recording = false;
	bool is_paused = false;
	std::vector<ObjectID> object_ids;
	std::vector<std::string> object_ids_strs;
	GuiVar<ObjectID> selected_object = ObjectID(0);
	GuiVar<bool> show_bone_visualisers = false;
	GuiVar<bool> show_collider_visualisers = false;
	std::unordered_map<EntityID, ObjectID> collider_visualiser_ids;
	std::string filter_text = std::string(1024, '\0');
};
