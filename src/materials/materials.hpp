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

struct Material
{
public:
	Material();
	
	static Material create_material(EMaterialType type);

public:
	std::string texture_path;
	EPipelineType pipeline_type = EPipelineType::COLOR;

	SDS::MaterialData material_data;
};