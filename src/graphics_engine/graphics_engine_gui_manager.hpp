#pragma once

#include "graphics_engine_base_module.hpp"
#include "gui/gui_manager.hpp"
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

	void update_fps(const float fps);

private:
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_graphics_engine;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_logical_device;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_physical_device;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_instance;
	using GraphicsEngineBaseModule<GraphicsEngineT>::create_buffer;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_num_swapchain_frames;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_render_pass;

public:
	using GuiManager<GameEngineT>::gui_windows;
	using GuiManager<GameEngineT>::graphic_settings;
	using GuiManager<GameEngineT>::object_spawner;
	using GuiManager<GameEngineT>::fps_counter;
};