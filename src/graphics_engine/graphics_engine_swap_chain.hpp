#pragma once

#include "graphics_engine_base_module.hpp"
#include "graphics_engine.hpp"


class GraphicsEngineSwapChain : public GraphicsEngineBaseModule
{
public:
	GraphicsEngineSwapChain(GraphicsEngine& engine) : GraphicsEngineBaseModule(engine) {}
	~GraphicsEngineSwapChain() {}

private:
};