#version 450

#include "../../library/library.glsl"

layout(location = 0) in vec3 in_position;
layout(location = 2) in vec2 in_tex_coord;

layout(location = 0) out vec2 frag_tex_coord;

layout(set=RASTERIZATION_HIGH_FREQ_PER_OBJ_SET_OFFSET, binding=RASTERIZATION_OBJECT_DATA_BINDING) uniform ObjectDataBuffer
{
	ObjectData data;
} object_data;

void main()
{
	gl_Position = object_data.data.mvp * vec4(in_position, 1.0);
	frag_tex_coord = in_tex_coord;
}
