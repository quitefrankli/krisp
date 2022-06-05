#include "gui_windows.ipp"
#include "game_engine.hpp"
#include "graphics_engine/graphics_engine.hpp"

using GameEngineT = GameEngine<GraphicsEngine>;


template class GuiGraphicsSettings<GameEngineT>;
template class GuiObjectSpawner<GameEngineT>;
template class GuiFPSCounter<GameEngineT>;
template class GuiMusic<GameEngineT>;