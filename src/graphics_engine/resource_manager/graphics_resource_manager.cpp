#include "graphics_resource_manager.ipp"
#include "graphics_buffer_manager.ipp"

#include "game_engine.hpp"


using GameEngineT = GameEngine<GraphicsEngine>;
using GraphicsEngineT = GameEngineT::GraphicsEngineT;

template class GraphicsResourceManager<GraphicsEngineT>;
template class GraphicsBufferManager<GraphicsEngineT>;