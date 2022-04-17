#include "graphics_engine_texture_manager.hpp"

#include "graphics_engine.hpp"

#include <iostream>


GraphicsEngineTextureManager::GraphicsEngineTextureManager(GraphicsEngine &engine) : GraphicsEngineBaseModule(engine)
{
}

GraphicsEngineTexture &GraphicsEngineTextureManager::create_new_unit(std::string texture_path)
{
	auto it = texture_units.find(texture_path);
	if (it != texture_units.end())
	{
		return it->second;
	}

	// note there is a bug that can cause emplace to still construct an object which is why
	// the above logic is required
	it = texture_units.emplace(std::piecewise_construct, 
									std::forward_as_tuple(texture_path), 
									std::forward_as_tuple(*this, texture_path)).first;
	return it->second;
}