#include "material_factory.hpp"
#include "maths.hpp"
#include "entity_component_system/material_system.hpp"

#include <unordered_map>


MaterialID MaterialFactory::fetch_preset(EMaterialPreset preset)
{
	static std::unordered_map<EMaterialPreset, MaterialID> material_map;
	if (material_map.contains(preset))
	{
		return material_map[preset];
	}

	ColorMaterial material;
	switch (preset)
	{
		case EMaterialPreset::ALWAYS_LIT:
		case EMaterialPreset::LIGHT_SOURCE:
			material.data.ambient = glm::vec3(0.0f);
			material.data.diffuse = glm::vec3(0.0f);
			material.data.specular = glm::vec3(0.0f);
			material.data.emissive = glm::vec3(1.0f, 1.0f, 0.0f);
			material.data.shininess = 1.0f;
			break;
		case EMaterialPreset::RUBBER:
			material.data.ambient = glm::vec3(0.05f, 0.05f, 0.0f);
			material.data.diffuse = glm::vec3(0.5f, 0.5f, 0.4f);
			material.data.specular = glm::vec3(0.7f, 0.7f, 0.04f);
			material.data.emissive = glm::vec3(0.0f);
			material.data.shininess = 0.078125f;
			break;
		case EMaterialPreset::PLASTIC:
			material.data.ambient = glm::vec3(0.0f, 0.0f, 0.0f);
			material.data.diffuse = glm::vec3(0.55f, 0.55f, 0.55f);
			material.data.specular = glm::vec3(0.7f, 0.7f, 0.7f);
			material.data.emissive = glm::vec3(0.0f);
			material.data.shininess = 0.25f;
			break;
		case EMaterialPreset::METAL:
			material.data.ambient = glm::vec3(0.05f, 0.05f, 0.05f);
			material.data.diffuse = glm::vec3(0.5f, 0.5f, 0.5f);
			material.data.specular = glm::vec3(0.7f, 0.7f, 0.7f);
			material.data.emissive = glm::vec3(0.0f);
			material.data.shininess = 0.78125f;
			break;
		case EMaterialPreset::GIZMO_ARROW:
			material.data.ambient = Maths::zero_vec;
			material.data.diffuse = Maths::zero_vec;
			material.data.specular = Maths::zero_vec;
			material.data.emissive = glm::vec3(1.0f, 0.0f, 0.0f);
			material.data.shininess = 1.0f;
			break;
		case EMaterialPreset::GIZMO_ARC:
			material.data.ambient = Maths::zero_vec;
			material.data.diffuse = Maths::zero_vec;
			material.data.specular = Maths::zero_vec;
			material.data.emissive = glm::vec3(0.0f, 1.0f, 1.0f);
			material.data.shininess = 1.0f;
			break;
		default:
			break;
	}

	auto material_ptr = std::make_unique<ColorMaterial>(std::move(material));
	const auto material_id = MaterialSystem::add(std::move(material_ptr));
	material_map[preset] = material_id;

	return material_id;
}