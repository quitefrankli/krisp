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
	LIGHTWEIGHT_OFFSCREEN_PIPELINE,
	SKINNED, // for skinned meshes

	// Internal Use

	_STENCIL_COLOR_VERTICES,
	_STENCIL_TEXTURE_VERTICES,
	_WIREFRAME_COLOR_VERTICES,
	_WIREFRAME_TEXTURE_VERTICES,
};