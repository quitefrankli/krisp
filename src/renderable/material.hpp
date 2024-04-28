#pragma once

#include "graphics_engine/pipeline/pipeline_types.hpp"
#include "shared_data_structures.hpp"
#include "identifications.hpp"

#include <glm/vec3.hpp>

#include <string_view>


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
	Material(Material&&) noexcept = default;
	
	MaterialID get_id() const { return id; }

public:
	MaterialTexture texture;
	EPipelineType pipeline_type = EPipelineType::COLOR;

	SDS::MaterialData material_data;

private:
	const MaterialID id = MaterialID::generate_new_id();
};