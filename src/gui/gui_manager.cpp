#include "gui_manager.hpp"
#include "game_engine.hpp"

#include <algorithm>


GuiManager::GuiManager(unsigned window_width) :
	graphic_settings(spawn_gui<GuiGraphicsSettings>()),
	object_spawner(spawn_gui<GuiObjectSpawner>()),
	fps_counter(spawn_gui<GuiFPSCounter>(window_width))
{
}

void GuiManager::process(GameEngine& engine)
{
	std::for_each(gui_windows.begin(), gui_windows.end(), [&engine](auto& gui) { gui->process(engine); });
}