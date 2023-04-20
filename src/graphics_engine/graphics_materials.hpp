#pragma once

#include "materials.hpp"
#include "shared_data_structures.hpp"
#include "graphics_engine_texture.hpp"

#include <vulkan/vulkan.h>

struct GraphicsMaterial
{
public:
	GraphicsMaterial(const Material& material, GraphicsEngineTexture* texture = nullptr) :
		texture(texture),
		data(material.material_data)
	{
	}

	GraphicsEngineTexture& get_texture() const
	{ 
		if (!texture)
		{
			throw std::runtime_error("GraphicsMaterial::get_texture() - texture is nullptr");
		}

		return *texture; 
	}

	const SDS::MaterialData& get_data() const { return data; }

private:
	GraphicsEngineTexture* texture;
	SDS::MaterialData data;
};