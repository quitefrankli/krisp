#pragma once


enum class ERenderType
{
	UNASSIGNED,
	STANDARD, // texture + color
	COLOR, // no texture
	// TEXTURE, // texture only, not supported yet
	CUBEMAP, // 3D texture for horizon, there is a bit of a bug with this mode and WIREFRAME mode, very minor though
	RAYTRACING,
	LIGHTWEIGHT_OFFSCREEN_PIPELINE,
	SKINNED, // for skinned meshes
	QUAD,
	PARTICLE, // billboard particles
};