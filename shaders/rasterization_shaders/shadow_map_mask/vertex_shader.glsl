#version 450

#extension GL_GOOGLE_include_directive : enable

#include "../../library/library.glsl"

layout(location=0) in vec3 in_position;
layout(location=2) in vec2 in_tex_coord;
layout(location=0) out vec3 world_pos;
layout(location=1) out vec2 tex_coord;

layout(set=RASTERIZATION_HIGH_FREQ_PER_OBJ_SET_OFFSET, binding=RASTERIZATION_OBJECT_DATA_BINDING) uniform ObjectDataBuffer
{
	ObjectData data;
} object_data;

void main()
{
	world_pos = (object_data.data.model * vec4(in_position, 1.0)).xyz;
	tex_coord = in_tex_coord;
	gl_Position = vec4(world_pos, 1.0);
}
