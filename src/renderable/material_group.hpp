#pragma once

#include "material.hpp"
#include "identifications.hpp"
#include "entity_component_system/material_system.hpp"

#include <cassert>
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
