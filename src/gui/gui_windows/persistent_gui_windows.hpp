#pragma once

#include "gui_windows.hpp"

class PersistentUiWindow : public EngineUiWindow
{
public:
	using EngineUiWindow::EngineUiWindow;
};

class GuiFPSCounter : public PersistentUiWindow
{
public:
	GuiFPSCounter();
	void process(GameEngine& engine) override;
	void draw() override;

private:
	float fps = 0.0f;
	float tps = 0.0f;
};
