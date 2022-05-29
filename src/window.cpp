#include "window.ipp"

#include "game_engine.hpp"
#include "graphics_engine/graphics_engine.hpp"

using GameEngineT = GameEngine<GraphicsEngine>;


template class App::Window<GameEngineT>;