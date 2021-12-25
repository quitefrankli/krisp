#pragma once

#include "graphics_engine_base_module.hpp"
#include "gui/gui.hpp"

#include <vector>
#include <memory>


class GraphicsEngineGui : public GraphicsEngineBaseModule
{
public:
	GraphicsEngineGui(GraphicsEngine& engine);
	~GraphicsEngineGui();

	void process();
	void add_render_cmd(VkCommandBuffer& cmd_buffer);
	void draw();
	void spawn_gui(std::shared_ptr<GuiWindow>& gui);	

	std::vector<std::shared_ptr<GuiWindow>> gui_windows; 
};