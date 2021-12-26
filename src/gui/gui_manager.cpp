#include "gui_manager.hpp"
#include "game_engine.hpp"

#include <algorithm>


GuiManager::GuiManager() :
	graphic_settings(spawn_gui<GuiGraphicsSettings>()),
	object_spawner(spawn_gui<GuiObjectSpawner>())
{
}

void GuiManager::process(GameEngine& engine)
{
	std::for_each(gui_windows.begin(), gui_windows.end(), [&engine](auto& gui) { gui->process(engine); });
}