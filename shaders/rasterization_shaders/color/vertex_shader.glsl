#version 450

#extension GL_GOOGLE_include_directive : enable

#include "../../library/library.glsl"

// keep in mind that some types such as dvec3 uses 2 slots therefore we need the next layout location to be 2 indices after
layout(location = 0) in vec3 in_position; // vertex pos
layout(location = 3) in vec3 in_normal; // vertex normal

layout(location=2) out vec3 surface_normal;
layout(location=4) out vec3 frag_pos;

layout(set=0, binding=0) uniform ObjectDataBuffer
{
	ObjectData data;
} object_data;

void main()
{
    gl_Position = object_data.data.mvp * vec4(in_position, 1.0);
    surface_normal = object_data.data.rot_mat * in_normal;
	frag_pos = (object_data.data.model * vec4(in_position, 1.0)).xyz;
}