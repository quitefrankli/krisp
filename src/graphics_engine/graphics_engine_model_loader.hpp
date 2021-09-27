#pragma once

#include "graphics_engine_base_module.hpp"


class GraphicsEngineModelLoader : public GraphicsEngineBaseModule
{
public:
	GraphicsEngineModelLoader(GraphicsEngine& engine);

	const std::string MODEL_PATH = "models/viking_room.obj";
};