#pragma once

#include "graphics_engine_base_module.hpp"
#include "gui/gui_manager.hpp"
#include "gui/application_ui_manager.hpp"
#include "gui/gui_windows/gui_windows.hpp"
#include "analytics.hpp"
#include "entity_component_system/material_system.hpp"

#include <vector>
#include <memory>
#include <filesystem>

#include <imgui.h>

#include <string>
#include <atomic>


class GraphicsEngineGuiManager : public GraphicsEngineBaseModule
{
public:
	GraphicsEngineGuiManager(GraphicsEngine& engine);
	~GraphicsEngineGuiManager();

	void add_render_cmd(VkCommandBuffer& cmd_buffer);
	void draw();
	void set_application_ui_manager(ApplicationUiManager* manager) { application_ui_manager = manager; }
	void set_ui_layers_active(bool engine_active, bool application_active)
	{
		engine_ui_active.store(engine_active, std::memory_order_release);
		application_ui_active.store(application_active, std::memory_order_release);
	}
	void setup_imgui();
	void draw_workspace();
	void draw_application_ui();
	void build_default_layout(ImGuiID dockspace_id, const ImVec2& size);
	// some gui_windows require setup in the graphics_engine thread
	void setup_gui_windows();
	
	void compose_texture_for_gui_window(std::string_view texture_filename, GuiPhotoBase& gui_photo);

	EngineUiManager& get_engine_ui_manager() { return engine_ui_manager; }
	const EngineUiManager& get_engine_ui_manager() const { return engine_ui_manager; }
	const GuiGraphicsSettings& get_graphic_settings() const
	{
		return engine_ui_manager.graphic_settings;
	}

private:
	bool reset_layout_requested = false;
	std::atomic<bool> engine_ui_active = true;
	std::atomic<bool> application_ui_active = false;
	// Registration topology is sealed before the graphics thread starts.
	ApplicationUiManager* application_ui_manager = nullptr;
	std::string imgui_ini_path;
	EngineUiManager engine_ui_manager;
	std::vector<MaterialHandle> gui_texture_owners;
};
