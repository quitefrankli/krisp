#include "material_group.hpp"

#include "entity_component_system/material_system.hpp"

#include <stdexcept>


TexturedMatGroup::TexturedMatGroup(const std::span<const MaterialHandle> mats) :
	material_owners(mats)
{
	bool has_base_color = false;
	for (const auto& owner : mats)
	{
		const auto id = owner->get_id();
		const auto* texture = dynamic_cast<const TextureMaterial*>(&owner->get());
		if (!texture)
			throw std::runtime_error("TexturedMatGroup: material is not a texture");

		auto set_optional = [id](std::optional<MaterialID>& slot)
		{
			if (slot)
				throw std::runtime_error("TexturedMatGroup: duplicate texture semantic");
			slot = id;
		};
		switch (texture->semantic)
		{
		case ETextureSemantic::BASE_COLOR:
			if (has_base_color)
				throw std::runtime_error("TexturedMatGroup: duplicate texture semantic");
			base_color_mat = id;
			has_base_color = true;
			break;
		case ETextureSemantic::NORMAL:
			set_optional(normal_mat);
			break;
		case ETextureSemantic::SPECULAR:
			set_optional(specular_mat);
			break;
		case ETextureSemantic::COUNT:
			throw std::runtime_error("TexturedMatGroup: invalid texture semantic");
		}
	}
	if (!has_base_color)
		throw std::runtime_error("TexturedMatGroup: base-color texture is required");
}

const MaterialHandle& TexturedMatGroup::get_material_owner(const MaterialID id) const
{
	for (const auto& owner : material_owners)
		if (owner->get_id() == id)
			return owner;
	throw std::runtime_error("TexturedMatGroup: material owner not found");
}
