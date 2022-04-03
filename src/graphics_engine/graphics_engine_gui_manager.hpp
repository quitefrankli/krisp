#pragma once

#include "graphics_engine_base_module.hpp"
#include "gui/gui_manager.hpp"
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

	void update_fps(const float fps);
};