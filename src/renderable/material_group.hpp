#pragma once

#include "material.hpp"
#include "identifications.hpp"
#include "entity_component_system/material_system.hpp"

#include <cassert>
#include <optional>
#include <span>


// This might not be necessary, can consider to be deleted
struct MaterialGroup
{
};

using MatVec = std::vector<MaterialID>;

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

struct TexturedMatGroup : public MaterialGroup
{
	TexturedMatGroup() = default;
	explicit TexturedMatGroup(std::span<const MaterialHandle> mats);
	const MaterialHandle& get_material_owner(MaterialID id) const;

	MatVec get_materials() const
	{
		MatVec materials{ base_color_mat };
		if (normal_mat.has_value())
			materials.push_back(*normal_mat);
		if (specular_mat.has_value())
			materials.push_back(*specular_mat);
		return materials;
	}

	MaterialID base_color_mat;
	std::optional<MaterialID> normal_mat;
	std::optional<MaterialID> specular_mat;

private:
	std::span<const MaterialHandle> material_owners;
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
