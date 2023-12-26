#version 450

#include "../../library/library.glsl"

// keep in mind that some types such as dvec3 uses 2 slots therefore we need the next layout location to be 2 indices after
layout(location = 0) in vec3 in_position;
layout(location = 2) in vec2 in_tex_coord; // unused

layout(location=0) out vec3 frag_tex_coord;

layout(set=RASTERIZATION_LOW_FREQ_SET_OFFSET, binding=RASTERIZATION_GLOBAL_DATA_BINDING) uniform GlobalDataBuffer
{
	GlobalData data;
} global_data;

void main()
{
	// vulkan depth goes from 0 -> 1.0 in screen space, setting the z value to 1 means it will always
	// be behind everything else
	mat4 untranslated_view = mat4(mat3(global_data.data.view));
	gl_Position = global_data.data.proj * untranslated_view * vec4(in_position, 1.0);
	gl_Position = gl_Position.xyzz;

	frag_tex_coord = in_position;
	// Convert cubemap coordinates into Vulkan coordinate space
	// frag_tex_coord.xy *= -1.0;
}