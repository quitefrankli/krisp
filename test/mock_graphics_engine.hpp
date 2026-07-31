#pragma once

#include <graphics_engine/engine_base.hpp>


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

	void request_shutdown() override { shutdown_requested = true; }

	EngineUiManager& get_gui_manager() override
	{
		return gui_manager;
	}
	void set_application_ui_manager(ApplicationUiManager* manager) override
	{
		application_ui_manager = manager;
	}
	void set_ui_layers_active(bool engine_active, bool application_active) override
	{
		engine_ui_active = engine_active;
		application_ui_active = application_active;
	}

	virtual float get_fps() const override { return 1.0f; }

	virtual void run() override {}
	ApplicationUiManager* application_ui_manager = nullptr;
	bool engine_ui_active = true;
	bool application_ui_active = false;
	bool shutdown_requested = false;

private:
	EngineUiManager gui_manager;
};
