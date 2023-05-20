#pragma once

#include "graphics_engine_base_module.hpp"
#include "gui/gui_manager.hpp"
#include "gui/gui_windows.hpp"
#include "analytics.hpp"

#include <vector>
#include <memory>


template<typename GraphicsEngineT, typename GameEngineT>
class GraphicsEngineGuiManager : public GraphicsEngineBaseModule<GraphicsEngineT>, public GuiManager<GameEngineT>
{
public:
	GraphicsEngineGuiManager(GraphicsEngineT& engine);
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

	const GuiGraphicsSettings<GameEngineT>& get_graphic_settings() const { return this->graphic_settings; }

private:
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_graphics_engine;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_logical_device;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_rsrc_mgr;
	using GraphicsEngineBaseModule<GraphicsEngineT>::create_buffer;
	using GuiManager<GameEngineT>::gui_windows;
	using GuiManager<GameEngineT>::graphic_settings;
	using GuiManager<GameEngineT>::object_spawner;
	using GuiManager<GameEngineT>::fps_counter;
};