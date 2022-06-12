#pragma once

#include "gui_windows.hpp"

#include <vector>
#include <memory>


template<typename GameEngineT>
class GuiManager
{
protected:
	std::vector<std::unique_ptr<GuiWindow<GameEngineT>>> gui_windows; 

public:
	GuiManager() :
		graphic_settings(spawn_gui<GuiGraphicsSettings<GameEngineT>>()),
		object_spawner(spawn_gui<GuiObjectSpawner<GameEngineT>>()),
		fps_counter(spawn_gui<GuiFPSCounter<GameEngineT>>())
	{
	}

	template<typename Gui_T, typename... Args>
	Gui_T& spawn_gui(Args&&... args)
	{
		gui_windows.push_back(std::make_unique<Gui_T>(std::forward<Args>(args)...));
		auto& gui = gui_windows.back();
		return *static_cast<Gui_T*>(gui.get());
	}

	// references the GuiManager::gui_windows
	GuiGraphicsSettings<GameEngineT>& graphic_settings;
	GuiObjectSpawner<GameEngineT>& object_spawner;
	GuiFPSCounter<GameEngineT>& fps_counter;

public: // for GameEngine
	void process(GameEngineT& engine)
	{
		for (auto& gui : gui_windows)
		{
			gui->process(engine);
		}
	}
};