#pragma once


enum class ERenderType
{
	STANDARD,
	COLOR, // no texture
	// TEXTURE, // texture only, not supported yet
	WIREFRAME,
	LIGHT_SOURCE,
	CUBEMAP // 3D texture for horizon, there is a bit of a bug with this mode and WIREFRAME mode, very minor though
};