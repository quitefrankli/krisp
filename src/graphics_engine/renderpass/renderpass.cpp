#include "renderpass.hpp"
#include "offscreen_renderpass.ipp"
#include "graphics_engine/graphics_engine.hpp"
#include "game_engine.hpp"


using GameEngineT = GameEngine<GraphicsEngine>;
using GraphicsEngineT = GameEngineT::GraphicsEngineT;

template class OffScreenRenderPass<GraphicsEngineT>;
