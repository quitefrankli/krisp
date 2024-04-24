#pragma once

#include <graphics_engine/engine_base.hpp>
#include <graphics_engine/graphics_engine_commands.hpp>


class MockGraphicsEngine : public GraphicsEngineBase
{
public:
    MockGraphicsEngine(GameEngine& engine)
    {
    }

    template<class T = int> 
	constexpr T get_window_width() const { return T(1980); }
	template<class T = int>
	constexpr T get_window_height() const { return T(1080); }

	void enqueue_cmd(std::unique_ptr<GraphicsEngineCommand>&& cmd) final {}

	GuiManager& get_gui_manager()
	{
		return gui_manager;
	}

	void handle_command(SpawnObjectCmd& cmd) final {}
	void handle_command(DeleteObjectCmd& cmd) final 
	{
		++num_objs_deleted;
	}
	void handle_command(StencilObjectCmd& cmd) final {}
	void handle_command(UnStencilObjectCmd& cmd) final {}
	void handle_command(ShutdownCmd& cmd) final {}
	void handle_command(ToggleWireFrameModeCmd& cmd) final {}
	void handle_command(UpdateCommandBufferCmd& cmd) final {}

	float get_fps() const final { return 1.0f; }

	uint64_t get_num_objs_deleted() const final { return num_objs_deleted; }

	void run() final {}

	void increment_num_objs_deleted() final { ++num_objs_deleted; }

	uint64_t num_objs_deleted = 0;

private:
	GuiManager gui_manager;
};