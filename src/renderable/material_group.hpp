#pragma once

#include "material.hpp"
#include "identifications.hpp"

#include <cassert>


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

struct TextureOnlyMatGroup : public MaterialGroup
{
	TextureOnlyMatGroup() = default;
	TextureOnlyMatGroup(MatVec mats)
	{
		assert(mats.size() == 1);
		texture_mat = mats[0];
	}

	MatVec get_materials() const
	{
		return { texture_mat };
	}

	MaterialID texture_mat;
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