#pragma once

#include "graphics_engine_base_module.hpp"
#include "gui/gui_manager.hpp"

#include <vector>
#include <memory>


class GraphicsEngineGuiManager : public GraphicsEngineBaseModule, public GuiManager
{
public:
	GraphicsEngineGuiManager(GraphicsEngine& engine);
	~GraphicsEngineGuiManager();

	void add_render_cmd(VkCommandBuffer& cmd_buffer);
	void draw();
};