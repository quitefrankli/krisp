#include "type_registry.hpp"

#include <array>

namespace
{
struct SceneObjectType
{
	std::string_view name;
	TypeRegistry::Factory create;
};

constexpr std::array scene_object_types{
	SceneObjectType{ Object::serialization_type_name, &TypeRegistry::DefaultFactory<Object>::create },
};

consteval bool names_are_unique()
{
	for (size_t left = 0; left < scene_object_types.size(); ++left)
		for (size_t right = left + 1; right < scene_object_types.size(); ++right)
			if (scene_object_types[left].name == scene_object_types[right].name)
				return false;
	return true;
}
static_assert(names_are_unique());

const SceneObjectType* find(const std::string_view name)
{
	for (const auto& type : scene_object_types)
		if (type.name == name)
			return &type;
	return nullptr;
}
}

bool TypeRegistry::contains(const std::string_view name)
{
	return find(name) != nullptr;
}

std::shared_ptr<Object> TypeRegistry::create(const std::string_view name)
{
	const auto* type = find(name);
	return type ? type->create() : nullptr;
}
