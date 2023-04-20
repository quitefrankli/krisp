#pragma once

#include "graphics_engine/pipeline/pipeline_types.hpp"
#include "shared_data_structures.hpp"

#include <glm/vec3.hpp>

#include <string_view>


struct Material
{
public:
	Material()
	{
		const glm::vec3 white = glm::vec3(1.0f);
		const glm::vec3 black = glm::vec3(0.0f);
		material_data.ambient = white;
		material_data.diffuse = white;
		material_data.specular = white;
		material_data.emissive = black;
		material_data.shininess = 32.0f;
	}

	std::string texture_path;
	EPipelineType pipeline_type = EPipelineType::COLOR;

	SDS::MaterialData material_data;
};