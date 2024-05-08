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
	DEFAULT
};

class MaterialFactory
{
public:
	static MaterialID fetch_preset(EMaterialPreset preset);
};