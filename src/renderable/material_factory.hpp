#pragma once

#include "identifications.hpp"
#include "renderable/material.hpp"


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
	static MaterialID fetch_preset(EMaterialPreset preset);
	static MaterialID fetch_white_texture();
};
