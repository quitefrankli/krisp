#pragma once

#include "gui_windows.hpp"

#include <vector>
#include <memory>


class GameEngine;

class GuiManager
{
protected:
	std::vector<std::unique_ptr<GuiWindow>> gui_windows; 

public:
	GuiManager();

	template<typename Gui_T, typename... Args>
	Gui_T& spawn_gui(Args&&... args)
	{
		gui_windows.push_back(std::make_unique<Gui_T>(std::forward<Args>(args)...));
		auto& gui = gui_windows.back();
		return *static_cast<Gui_T*>(gui.get());
	}

	GuiGraphicsSettings& graphic_settings;
	GuiObjectSpawner& object_spawner;

public: // for GameEngine
	void process(GameEngine& engine);
};