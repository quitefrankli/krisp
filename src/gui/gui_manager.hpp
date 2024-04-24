#pragma once

#include "gui_windows.hpp"

#include <vector>
#include <memory>


class GuiManager
{
protected:
	std::vector<std::unique_ptr<GuiWindow>> gui_windows; 

public:
	GuiManager() :
		graphic_settings(spawn_gui<GuiGraphicsSettings>()),
		object_spawner(spawn_gui<GuiObjectSpawner>()),
		fps_counter(spawn_gui<GuiFPSCounter>()),
		statistics(spawn_gui<GuiStatistics>()),
		debug(spawn_gui<GuiDebug>()),
		photo(spawn_gui<GuiPhoto>()),
		render_slicer(spawn_gui<GuiRenderSlicer>())
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
	GuiGraphicsSettings& graphic_settings;
	GuiObjectSpawner& object_spawner;
	GuiFPSCounter& fps_counter;
	GuiStatistics& statistics;
	GuiDebug& debug;
	GuiPhoto& photo;
	GuiRenderSlicer& render_slicer;

public: // for GameEngine
	void process(GameEngine& engine)
	{
		for (auto& gui : gui_windows)
		{
			gui->process(engine);
		}
	}
};