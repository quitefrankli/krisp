#version 450

#include "../../library/library.glsl"

layout(location=0) in vec3 in_position;
layout(location=0) out vec3 world_pos;

layout(set=RASTERIZATION_PER_RENDERABLE_FRAME_SET_OFFSET, binding=RASTERIZATION_OBJECT_DATA_BINDING) uniform ObjectDataBuffer
{
	ObjectData data;
} object_data;

void main()
{
	world_pos = (object_data.data.model * vec4(in_position, 1.0)).xyz;
	gl_Position = vec4(world_pos, 1.0);
}
