#include "material_factory.hpp"
#include "entity_component_system/material_system.hpp"

#include <unordered_map>


MaterialID MaterialFactory::create_id(EMaterialType preset)
{
	static std::unordered_map<EMaterialType, MaterialID> material_map;
	if (material_map.contains(preset))
	{
		return material_map[preset];
	}

	auto material = std::make_unique<Material>(Material::create_material(preset));
	const auto material_id = MaterialSystem::add(std::move(material));
	material_map[preset] = material_id;

	return material_id;
}