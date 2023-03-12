#include "graphics_engine.ipp"
#include "graphics_engine_buffers.ipp"
#include "graphics_engine_instance.ipp"
#include "engine_commands_handler.ipp"
#include "game_engine.hpp"


using GameEngineT = GameEngine<GraphicsEngine>;

template class GraphicsEngine<GameEngineT>;