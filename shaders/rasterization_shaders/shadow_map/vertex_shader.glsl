#version 450

#extension GL_GOOGLE_include_directive : enable

#include "../../library/library.glsl"

// keep in mind that some types such as dvec3 uses 2 slots therefore we need the next layout location to be 2 indices after
layout(location=0) in vec3 in_position; // vertex pos
layout(location=0) out vec3 world_pos;

layout(set=RASTERIZATION_HIGH_FREQ_PER_OBJ_SET_OFFSET, binding=RASTERIZATION_OBJECT_DATA_BINDING) uniform ObjectDataBuffer
{
	ObjectData data;
} object_data;

void main()
{
	world_pos = (object_data.data.model * vec4(in_position, 1.0)).xyz;
	gl_Position = vec4(world_pos, 1.0);
}
