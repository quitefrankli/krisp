#pragma once

#include "graphics_engine_base_module.hpp"
#include "graphics_engine_texture.hpp"
#include "utility.hpp"

#include <quill/Quill.h>

#include <unordered_map>
#include <string>


template<typename GraphicsEngineT>
class GraphicsEngineTextureManager : public GraphicsEngineBaseModule<GraphicsEngineT>
{
public:
	GraphicsEngineTextureManager(GraphicsEngineT& engine) : GraphicsEngineBaseModule<GraphicsEngineT>(engine)
	{
		LOG_INFO(Utility::get().get_logger(),
				 "GraphicsEngineTextureManager: chosen max anisotropy:={}\n",
				 engine.get_device_module().get_physical_device_properties().properties.limits.maxSamplerAnisotropy);
	}

	GraphicsEngineTexture<GraphicsEngineT>& create_new_unit(std::string texture_path)
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

private:
	// note that this is not deleted even when an object referencing this texture gets destroyed
	std::unordered_map<std::string, GraphicsEngineTexture<GraphicsEngineT>> texture_units;
};
