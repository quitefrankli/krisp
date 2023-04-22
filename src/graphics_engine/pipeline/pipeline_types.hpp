#pragma once


enum class EPipelineType
{
	STANDARD, // texture + color
	COLOR, // no texture
	// TEXTURE, // texture only, not supported yet
	WIREFRAME,
	CUBEMAP, // 3D texture for horizon, there is a bit of a bug with this mode and WIREFRAME mode, very minor though
	STENCIL, // for object highlighting
	RAYTRACING,
	LIGHTWEIGHT_OFFSCREEN_PIPELINE
};