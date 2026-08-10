#version 450

#include "../../library/library.glsl"

layout(location=0) in vec3 in_position;
layout(location=1) in vec3 in_normal;
layout(location=2) in vec2 in_tex_coord;
layout(location=0) out vec2 tex_coord;

layout(set=RASTERIZATION_PER_RENDERABLE_FRAME_SET_OFFSET, binding=RASTERIZATION_OBJECT_DATA_BINDING) uniform ObjectDataBuffer
{
	ObjectData data;
} object_data;
layout(set=RASTERIZATION_LOW_FREQ_SET_OFFSET, binding=RASTERIZATION_GLOBAL_DATA_BINDING) uniform GlobalDataBuffer
{
	GlobalData data;
} global_data;

void main()
{
	const vec3 world_position = (object_data.data.model * vec4(in_position, 1.0)).xyz;
	const vec3 world_normal = normalize(transpose(inverse(mat3(object_data.data.model))) * in_normal);
	tex_coord = in_tex_coord;
	gl_Position = global_data.data.proj * global_data.data.view
		* vec4(world_position + world_normal * STENCIL_OFFSET, 1.0);
}
