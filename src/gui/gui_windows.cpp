#include "gui_windows.ipp"
#include "game_engine.hpp"
#include "graphics_engine/graphics_engine.hpp"

using GameEngineT = GameEngine<GraphicsEngine>;


template class GuiGraphicsSettings<GameEngineT>;
template class GuiObjectSpawner<GameEngineT>;
template class GuiFPSCounter<GameEngineT>;
template class GuiMusic<GameEngineT>;
template class GuiStatistics<GameEngineT>;
template class GuiDebug<GameEngineT>;
template class GuiPhoto<GameEngineT>;