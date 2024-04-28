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
	GIZMO_ARROW,
	GIZMO_ARC
};

class MaterialFactory
{
public:
	static MaterialID fetch_preset(EMaterialPreset preset);
};