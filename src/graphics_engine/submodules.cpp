#include "graphics_engine_base_module.ipp"
#include "graphics_engine_device.ipp"
#include "graphics_engine_frame.ipp"
#include "graphics_engine_gui_manager.ipp"
#include "graphics_engine_object.ipp"
#include "graphics_engine_swap_chain.ipp"
#include "graphics_engine_texture.ipp"
#include "graphics_engine_validation_layer.ipp"
#include "graphics_engine_instance.ipp"
#include "graphics_engine_texture_manager.ipp"
#include "raytracing.ipp"

#include "game_engine.hpp"


using GameEngineT = GameEngine<GraphicsEngine>;
using GraphicsEngineT = GameEngineT::GraphicsEngineT;

template class GraphicsEngineBaseModule<GraphicsEngineT>;
template class GraphicsEngineDevice<GraphicsEngineT>;
template class GraphicsEngineFrame<GraphicsEngineT>;
template class GraphicsEngineGuiManager<GraphicsEngineT, GameEngineT>;
template class GraphicsEngineObject<GraphicsEngineT>;
template class GraphicsEngineObjectPtr<GraphicsEngineT>;
template class GraphicsEngineObjectRef<GraphicsEngineT>;
template class GraphicsEngineSwapChain<GraphicsEngineT>;
template class GraphicsEngineValidationLayer<GraphicsEngineT>;
template class GraphicsEngineRayTracing<GraphicsEngineT>;
template class GraphicsEngineInstance<GraphicsEngineT>;
template class GraphicsEngineTextureManager<GraphicsEngineT>;