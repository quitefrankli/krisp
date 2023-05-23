#version 450

#include "../../library/library.glsl"

// keep in mind that some types such as dvec3 uses 2 slots therefore we need the next layout location to be 2 indices after
layout(location = 0) in vec3 in_position;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;

layout(location=0) out vec2 frag_tex_coord;
layout(location=1) out vec3 surface_normal;
layout(location=2) out vec3 frag_pos;

layout(set=RASTERIZATION_HIGH_FREQ_PER_OBJ_SET_OFFSET, binding=RASTERIZATION_OBJECT_DATA_BINDING) uniform ObjectDataBuffer
{
	ObjectData data;
} object_data;

layout(set=RASTERIZATION_LOW_FREQ_SET_OFFSET, binding=RASTERIZATION_GLOBAL_DATA_BINDING) uniform GlobalDataBuffer
{
	GlobalData data;
} global_data;

vec3 get_light_normal()
{
	return normalize(global_data.data.light_pos - object_data.data.rot_mat * in_position);
}

void main()
{
	gl_Position = object_data.data.mvp * vec4(in_position, 1.0);

	frag_tex_coord = inTexCoord;
	surface_normal = object_data.data.rot_mat * inNormal;
	// it's likely we can remove the need for global_data.data.view_pos and compute everything in "view space"
	frag_pos = (object_data.data.model * vec4(in_position, 1.0)).xyz;
}