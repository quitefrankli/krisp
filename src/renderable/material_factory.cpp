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

	Material material;
	switch (preset)
	{
		case EMaterialPreset::ALWAYS_LIT:
		case EMaterialPreset::LIGHT_SOURCE:
			material.material_data.ambient = glm::vec3(0.0f);
			material.material_data.diffuse = glm::vec3(0.0f);
			material.material_data.specular = glm::vec3(0.0f);
			material.material_data.emissive = glm::vec3(1.0f, 1.0f, 0.0f);
			material.material_data.shininess = 1.0f;
			break;
		case EMaterialPreset::RUBBER:
			material.material_data.ambient = glm::vec3(0.05f, 0.05f, 0.0f);
			material.material_data.diffuse = glm::vec3(0.5f, 0.5f, 0.4f);
			material.material_data.specular = glm::vec3(0.7f, 0.7f, 0.04f);
			material.material_data.emissive = glm::vec3(0.0f);
			material.material_data.shininess = 0.078125f;
			break;
		case EMaterialPreset::PLASTIC:
			material.material_data.ambient = glm::vec3(0.0f, 0.0f, 0.0f);
			material.material_data.diffuse = glm::vec3(0.55f, 0.55f, 0.55f);
			material.material_data.specular = glm::vec3(0.7f, 0.7f, 0.7f);
			material.material_data.emissive = glm::vec3(0.0f);
			material.material_data.shininess = 0.25f;
			break;
		case EMaterialPreset::METAL:
			material.material_data.ambient = glm::vec3(0.05f, 0.05f, 0.05f);
			material.material_data.diffuse = glm::vec3(0.5f, 0.5f, 0.5f);
			material.material_data.specular = glm::vec3(0.7f, 0.7f, 0.7f);
			material.material_data.emissive = glm::vec3(0.0f);
			material.material_data.shininess = 0.78125f;
			break;
		case EMaterialPreset::GIZMO_ARROW:
			material.material_data.ambient = Maths::zero_vec;
			material.material_data.diffuse = Maths::zero_vec;
			material.material_data.specular = Maths::zero_vec;
			material.material_data.emissive = glm::vec3(1.0f, 0.0f, 0.0f);
			material.material_data.shininess = 1.0f;
			break;
		case EMaterialPreset::GIZMO_ARC:
			material.material_data.ambient = Maths::zero_vec;
			material.material_data.diffuse = Maths::zero_vec;
			material.material_data.specular = Maths::zero_vec;
			material.material_data.emissive = glm::vec3(0.0f, 1.0f, 1.0f);
			material.material_data.shininess = 1.0f;
			break;
		default:
			break;
	}

	auto material_ptr = std::make_unique<Material>(std::move(material));
	const auto material_id = MaterialSystem::add(std::move(material_ptr));
	material_map[preset] = material_id;

	return material_id;
}