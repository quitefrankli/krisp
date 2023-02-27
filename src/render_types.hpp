#pragma once


enum class ERenderType
{
	STANDARD, // texture + color
	COLOR, // no texture
	// TEXTURE, // texture only, not supported yet
	COLOR_NO_LIGHTING,
	WIREFRAME,
	LIGHT_SOURCE,
	CUBEMAP, // 3D texture for horizon, there is a bit of a bug with this mode and WIREFRAME mode, very minor though
	STENCIL, // for object highlighting
};