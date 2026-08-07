#pragma once

#include "material.hpp"
#include "identifications.hpp"
#include "entity_component_system/material_system.hpp"

#include <algorithm>
#include <cassert>
#include <span>
#include <stdexcept>


// This might not be necessary, can consider to be deleted
struct MaterialGroup
{
};

using MatVec = std::vector<MaterialID>;

// One PBR definition plus the texture resources referenced by its optional
// slots. Renderables retain the owners; bindings inside PbrMaterial remain
// compact, non-owning identities.
struct PbrMatGroup : public MaterialGroup
{
	explicit PbrMatGroup(const std::span<const MaterialHandle> owners) : owners(owners)
	{
		if (owners.empty() || !dynamic_cast<const PbrMaterial*>(&owners.front()->get()))
			throw std::invalid_argument("PBR material group must begin with a PbrMaterial");
		const auto& material = pbr();
		validate(material.textures.base_color, ETextureSemantic::BASE_COLOR);
		validate(material.textures.metallic_roughness, ETextureSemantic::METALLIC_ROUGHNESS);
		validate(material.textures.normal, ETextureSemantic::NORMAL);
	}

	const PbrMaterial& pbr() const
	{
		return static_cast<const PbrMaterial&>(owners.front()->get());
	}

	const MaterialHandle& texture_owner(const PbrMaterial::TextureBinding& binding) const
	{
		const auto found = std::ranges::find_if(owners, [&binding](const MaterialHandle& owner)
		{
			return owner->get_id() == binding.texture;
		});
		if (found == owners.end())
			throw std::invalid_argument("PBR texture binding has no retained owner");
		return *found;
	}

private:
	void validate(
		const std::optional<PbrMaterial::TextureBinding>& binding,
		const ETextureSemantic semantic) const
	{
		if (!binding)
			return;
		const auto& owner = texture_owner(*binding);
		const auto* texture = dynamic_cast<const TextureMaterial*>(&owner->get());
		if (!texture || texture->semantic != semantic)
			throw std::invalid_argument("PBR texture binding has the wrong semantic");
	}

	std::span<const MaterialHandle> owners;
};

// No textures
struct FlatMatGroup : public MaterialGroup
{
	FlatMatGroup() = default;
	FlatMatGroup(const std::span<const MaterialHandle> mats)
	{
		assert(mats.size() == 1);
		color_mat = mats[0]->get_id();
	}

	MatVec get_materials() const
	{
		return { color_mat };
	}

	MaterialID color_mat;
};

struct CubeMapMatGroup : public MaterialGroup
{
	CubeMapMatGroup() = default;
	explicit CubeMapMatGroup(const std::span<const MaterialHandle> mats)
	{
		assert(mats.size() == 6);
		material_owners = mats;
	}

	size_t size() const
	{
		return material_owners.size();
	}

	MaterialID get_material_id(size_t index) const
	{
		return material_owners[index]->get_id();
	}

	const Material& get_material(size_t index) const
	{
		return material_owners[index]->get();
	}

	std::span<const MaterialHandle> material_owners;
};
