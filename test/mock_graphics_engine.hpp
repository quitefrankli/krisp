#pragma once

#include <graphics_engine/engine_base.hpp>
#include <graphics_engine/graphics_engine_commands.hpp>


class MockGraphicsEngine : public GraphicsEngineBase
{
public:
	MockGraphicsEngine() = default;
    MockGraphicsEngine(GameEngine& engine)
    {
    }

	virtual ~MockGraphicsEngine() override = default;

    template<class T = int> 
	constexpr T get_window_width() const { return T(1980); }
	template<class T = int>
	constexpr T get_window_height() const { return T(1080); }

	virtual void enqueue_cmd(std::unique_ptr<GraphicsEngineCommand>&& cmd) override
	{
		cmd->process(this);
	}

	GuiManager& get_gui_manager()
	{
		return gui_manager;
	}

	virtual void handle_command(SpawnObjectCmd& cmd) override {}
	virtual void handle_command(DeleteObjectCmd& cmd) override 
	{
		++num_objs_deleted;
	}
	virtual void handle_command(StencilObjectCmd& cmd) override {}
	virtual void handle_command(UnStencilObjectCmd& cmd) override {}
	virtual void handle_command(ShutdownCmd& cmd) override {}
	virtual void handle_command(ToggleWireFrameModeCmd& cmd) override {}
	virtual void handle_command(UpdateCommandBufferCmd& cmd) override {}

	virtual float get_fps() const override { return 1.0f; }

	virtual uint64_t get_num_objs_deleted() const override { return num_objs_deleted; }

	virtual void run() override {}

	virtual void increment_num_objs_deleted() override { ++num_objs_deleted; }

	uint64_t num_objs_deleted = 0;

private:
	GuiManager gui_manager;
};