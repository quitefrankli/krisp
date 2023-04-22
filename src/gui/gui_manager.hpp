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
		fps_counter(spawn_gui<GuiFPSCounter<GameEngineT>>()),
		statistics(spawn_gui<GuiStatistics<GameEngineT>>()),
		debug(spawn_gui<GuiDebug<GameEngineT>>()),
		photo(spawn_gui<GuiPhoto<GameEngineT>>())
	{
	}

	template<typename Gui_T, typename... Args>
	Gui_T& spawn_gui(Args&&... args)
	{
		gui_windows.push_back(std::make_unique<Gui_T>(std::forward<Args>(args)...));
		auto& gui = gui_windows.back();
		return *static_cast<Gui_T*>(gui.get());
	}

	void update_buffer_capacities(const std::vector<std::pair<size_t, size_t>>& capacities)
	{
		statistics.update_buffer_capacities(capacities);
	}

	// references the GuiManager::gui_windows
	GuiGraphicsSettings<GameEngineT>& graphic_settings;
	GuiObjectSpawner<GameEngineT>& object_spawner;
	GuiFPSCounter<GameEngineT>& fps_counter;
	GuiStatistics<GameEngineT>& statistics;
	GuiDebug<GameEngineT>& debug;
	GuiPhoto<GameEngineT>& photo;

public: // for GameEngine
	void process(GameEngineT& engine)
	{
		for (auto& gui : gui_windows)
		{
			gui->process(engine);
		}
	}
};