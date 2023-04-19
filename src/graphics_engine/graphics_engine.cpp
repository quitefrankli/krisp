#include "graphics_engine.ipp"
#include "engine_commands_handler.ipp"
#include "game_engine.hpp"


using GameEngineT = GameEngine<GraphicsEngine>;

template class GraphicsEngine<GameEngineT>;