#pragma once

#include "graphics_engine_base_module.hpp"


class GraphicsEngineGui : public GraphicsEngineBaseModule
{
public:
	GraphicsEngineGui(GraphicsEngine& engine);
	~GraphicsEngineGui();

	void process();
	void add_render_cmd(VkCommandBuffer& cmd_buffer);
	void draw();
};