#pragma once

#include "material.hpp"
#include "identifications.hpp"

#include <cassert>
#include <optional>


// This might not be necessary, can consider to be deleted
struct MaterialGroup
{
};

using MatVec = std::vector<MaterialID>;

// No textures
struct FlatMatGroup : public MaterialGroup
{
	FlatMatGroup() = default;
	FlatMatGroup(MatVec mats)
	{
		assert(mats.size() == 1);
		color_mat = mats[0];
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
	explicit TexturedMatGroup(const MatVec& mats);

	MatVec get_materials() const
	{
		MatVec materials{ base_color_mat };
		if (normal_mat.has_value())
			materials.push_back(*normal_mat);
		if (specular_strength_mat.has_value())
			materials.push_back(*specular_strength_mat);
		if (specular_color_mat.has_value())
			materials.push_back(*specular_color_mat);
		return materials;
	}

	MaterialID base_color_mat;
	std::optional<MaterialID> normal_mat;
	std::optional<MaterialID> specular_strength_mat;
	std::optional<MaterialID> specular_color_mat;
};

struct CubeMapMatGroup : public MaterialGroup
{
	CubeMapMatGroup() = default;
	CubeMapMatGroup(MatVec mats)
	{
		assert(mats.size() == 6);
		cube_map_mats = mats;
	}

	MatVec get_materials() const
	{
		return cube_map_mats;
	}

	std::vector<MaterialID> cube_map_mats;
};
