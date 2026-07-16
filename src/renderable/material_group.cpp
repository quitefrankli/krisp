#include "material_group.hpp"

#include "entity_component_system/material_system.hpp"

#include <stdexcept>


TexturedMatGroup::TexturedMatGroup(const MatVec& mats)
{
	bool has_base_color = false;
	for (const auto id : mats)
	{
		const auto* texture = dynamic_cast<const TextureMaterial*>(&MaterialSystem::get(id));
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
		case ETextureSemantic::SPECULAR_STRENGTH:
			set_optional(specular_strength_mat);
			break;
		case ETextureSemantic::SPECULAR_COLOR:
			set_optional(specular_color_mat);
			break;
		case ETextureSemantic::COUNT:
			throw std::runtime_error("TexturedMatGroup: invalid texture semantic");
		}
	}
	if (!has_base_color)
		throw std::runtime_error("TexturedMatGroup: base-color texture is required");
}
