#pragma once

#include "material.hpp"


enum class EMaterialPreset
{
	ALWAYS_LIT,
	LIGHT_SOURCE,
	RUBBER,
	PLASTIC,
	METAL,
	DIFFUSE,
	GIZMO_ARROW,
	GIZMO_ARC,
	GIZMO_X_AXIS,
	GIZMO_Y_AXIS,
	GIZMO_Z_AXIS,
	GIZMO_UNIFORM_SCALE,
	DEFAULT
};

class MaterialFactory
{
public:
	static std::unique_ptr<Material> fetch_preset(EMaterialPreset preset);
	static std::unique_ptr<Material> fetch_white_texture();
	static std::unique_ptr<Material> fetch_black_texture();
};
