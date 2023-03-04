#pragma once

#include <graphics_engine/engine_base.hpp>
#include <graphics_engine/graphics_engine_commands.hpp>
#include <graphics_engine/graphics_engine_gui_manager.ipp>
#include <gui/gui_windows.ipp>


template<typename GameEngineT>
class MockGraphicsEngine : public GraphicsEngineBase
{
public:
    MockGraphicsEngine(GameEngineT& engine)
    {
    }

    template<class T = int> 
	constexpr T get_window_width() const { return T(1980); }
	template<class T = int>
	constexpr T get_window_height() const { return T(1080); }

	void enqueue_cmd(std::unique_ptr<GraphicsEngineCommand>&& cmd) {}

	GuiManager<GameEngineT>& get_gui_manager()
	{
		return gui_manager;
	}

	void handle_command(SpawnObjectCmd& cmd) final {}
	void handle_command(DeleteObjectCmd& cmd) final {}
	void handle_command(StencilObjectCmd& cmd) final {}
	void handle_command(UnStencilObjectCmd& cmd) final {}
	void handle_command(ShutdownCmd& cmd) final {}
	void handle_command(ToggleWireFrameModeCmd& cmd) final {}
	void handle_command(UpdateCommandBufferCmd& cmd) final {}

	float get_fps() const { return 1.0f; }

private:
	GuiManager<GameEngineT> gui_manager;
};