#pragma once

#include "gui_windows.hpp"
#include "entity_component_system/material_system.hpp"

#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <atomic>

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

	std::optional<uint32_t> consume_start_recording_request()
	{
		if (!should_start_recording.exchange(false, std::memory_order_acq_rel))
			return std::nullopt;
		return static_cast<uint32_t>(recording_fps);
	}

	bool consume_stop_recording_request()
	{
		return should_stop_recording.exchange(false, std::memory_order_acq_rel);
	}

	void set_is_recording(bool value)
	{
		is_recording.store(value, std::memory_order_release);
	}
	void request_recording_toggle()
	{
		if (is_recording.load(std::memory_order_acquire))
		{
			should_stop_recording.store(true, std::memory_order_release);
			should_start_recording.store(false, std::memory_order_release);
		}
		else
		{
			should_start_recording.store(true, std::memory_order_release);
			should_stop_recording.store(false, std::memory_order_release);
		}
	}

private:
	struct PhysicsVisual { ObjectID object; uint32_t body_id; };
	void refresh_physics_visualiser(GameEngine& engine);
	void clear_physics_visualiser(GameEngine& engine);

	bool should_refresh_objects_list = false;
	bool should_toggle_pause = false;
	bool should_take_screenshot = false;
	std::atomic<bool> should_start_recording = false;
	std::atomic<bool> should_stop_recording = false;
	std::atomic<bool> is_recording = false;
	int recording_fps = 60;
	bool is_paused = false;
	std::vector<ObjectID> object_ids;
	std::vector<std::string> object_ids_strs;
	GuiVar<ObjectID> selected_object = ObjectID(0);
	GuiVar<bool> show_bone_visualisers = false;
	GuiVar<bool> show_collider_visualisers = false;
	std::unordered_map<EntityID, PhysicsVisual> physics_visuals;
	std::unordered_set<EntityID> suppressed_physics_visuals;
	std::vector<MaterialHandle> physics_visual_materials;
	std::string filter_text = std::string(1024, '\0');
};
