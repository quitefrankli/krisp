#pragma once

#include "graphics_engine/pipeline/pipeline_types.hpp"

#include <glm/vec3.hpp>

#include <string_view>


struct Material
{
public:
	std::string texture_path;
	glm::vec3 color;
	float roughness = 0;
	float specular = 0;
	EPipelineType pipeline_type = EPipelineType::COLOR;
};