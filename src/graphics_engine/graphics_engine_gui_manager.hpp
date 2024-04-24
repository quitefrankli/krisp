#pragma once

#include "graphics_engine_base_module.hpp"
#include "gui/gui_manager.hpp"
#include "gui/gui_windows.hpp"
#include "analytics.hpp"

#include <vector>
#include <memory>


class GraphicsEngineGuiManager : public GraphicsEngineBaseModule, public GuiManager
{
public:
	GraphicsEngineGuiManager(GraphicsEngine& engine);
	~GraphicsEngineGuiManager();

	void add_render_cmd(VkCommandBuffer& cmd_buffer);
	void draw();
	void update_preview_window(
		GuiPhotoBase& gui, 
		VkSampler sampler, 
		VkImageView image_view, 
		const glm::uvec2& dimensions);

	void setup_imgui();
	// some gui_windows require setup in the graphics_engine thread
	void setup_gui_windows();
	
	void compose_texture_for_gui_window(const std::string_view texture_path, GuiPhotoBase& gui_photo);

	const GuiGraphicsSettings& get_graphic_settings() const { return this->graphic_settings; }

private:
	using GuiManager::gui_windows;
	using GuiManager::graphic_settings;
	using GuiManager::object_spawner;
	using GuiManager::fps_counter;
};