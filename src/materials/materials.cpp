#include "materials/materials.hpp"
#include "maths.hpp"


Material::Material()
{
	const glm::vec3 white = glm::vec3(1.0f);
	const glm::vec3 black = glm::vec3(0.0f);
	material_data.ambient = white;
	material_data.diffuse = white;
	material_data.specular = white;
	material_data.emissive = black;
	material_data.shininess = 32.0f;
}

Material Material::create_material(EMaterialType type)
{
	const glm::vec3 default_color = glm::vec3(1.0f);
	Material material;
	switch (type)
	{
		case EMaterialType::ALWAYS_LIT:
		case EMaterialType::LIGHT_SOURCE:
			material.material_data.ambient = glm::vec3(0.0f);
			material.material_data.diffuse = glm::vec3(0.0f);
			material.material_data.specular = glm::vec3(0.0f);
			material.material_data.emissive = glm::vec3(1.0f, 1.0f, 0.0f);
			material.material_data.shininess = 1.0f;
			return material;
		case EMaterialType::RUBBER:
			material.material_data.ambient = glm::vec3(0.05f, 0.05f, 0.0f);
			material.material_data.diffuse = glm::vec3(0.5f, 0.5f, 0.4f);
			material.material_data.specular = glm::vec3(0.7f, 0.7f, 0.04f);
			material.material_data.emissive = glm::vec3(0.0f);
			material.material_data.shininess = 0.078125f;
			return material;
		case EMaterialType::PLASTIC:
			material.material_data.ambient = glm::vec3(0.0f, 0.0f, 0.0f);
			material.material_data.diffuse = glm::vec3(0.55f, 0.55f, 0.55f);
			material.material_data.specular = glm::vec3(0.7f, 0.7f, 0.7f);
			material.material_data.emissive = glm::vec3(0.0f);
			material.material_data.shininess = 0.25f;
			return material;
		case EMaterialType::METAL:
			material.material_data.ambient = glm::vec3(0.05f, 0.05f, 0.05f);
			material.material_data.diffuse = glm::vec3(0.5f, 0.5f, 0.5f);
			material.material_data.specular = glm::vec3(0.7f, 0.7f, 0.7f);
			material.material_data.emissive = glm::vec3(0.0f);
			material.material_data.shininess = 0.78125f;
			return material;
		case EMaterialType::GIZMO_ARROW:
			material.material_data.ambient = Maths::zero_vec;
			material.material_data.diffuse = Maths::zero_vec;
			material.material_data.specular = Maths::zero_vec;
			material.material_data.emissive = glm::vec3(1.0f, 0.0f, 0.0f);
			material.material_data.shininess = 1.0f;
			return material;
		case EMaterialType::GIZMO_ARC:
			material.material_data.ambient = Maths::zero_vec;
			material.material_data.diffuse = Maths::zero_vec;
			material.material_data.specular = Maths::zero_vec;
			material.material_data.emissive = glm::vec3(0.0f, 1.0f, 1.0f);
			material.material_data.shininess = 1.0f;
			return material;
		default:
			return Material();
	}
}
