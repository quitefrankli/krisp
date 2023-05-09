#pragma once

#include "graphics_engine/pipeline/pipeline_types.hpp"
#include "shared_data_structures.hpp"

#include <glm/vec3.hpp>

#include <string_view>


enum class EMaterialType
{
	ALWAYS_LIT,
	LIGHT_SOURCE,
	RUBBER,
	PLASTIC,
	METAL,
	GIZMO_ARROW,
	GIZMO_ARC
};

struct MaterialTexture
{
	std::byte* data = nullptr; // not owned by this struct
	size_t data_len = 0;
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t channels = 4; 
	uint32_t texture_id = 0;
};

struct Material
{
public:
	Material();
	
	static Material create_material(EMaterialType type);

public:
	MaterialTexture texture;
	EPipelineType pipeline_type = EPipelineType::COLOR;

	SDS::MaterialData material_data;
};