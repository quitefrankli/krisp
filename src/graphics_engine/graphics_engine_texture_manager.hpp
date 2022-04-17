#pragma once

#include "graphics_engine_base_module.hpp"
#include "graphics_engine_texture.hpp"

#include <unordered_map>
#include <string>


class GraphicsEngineTextureManager : public GraphicsEngineBaseModule
{
public:
	GraphicsEngineTextureManager(GraphicsEngine& engine);

	GraphicsEngineTexture& create_new_unit(std::string texture_path);

private:
	// note that this is not deleted even when an object referencing this texture gets destroyed
	std::unordered_map<std::string, GraphicsEngineTexture> texture_units;
};