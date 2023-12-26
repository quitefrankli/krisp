#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "../../library/library.glsl"

layout(location=0) in vec3 frag_tex_coord;

layout(location = 0) out vec4 out_color;

layout(set=RASTERIZATION_HIGH_FREQ_PER_SHAPE_SET_OFFSET, 
	   binding=RASTERIZATION_ALBEDO_TEXTURE_DATA_BINDING) uniform samplerCube cubemap_sampler;

void main()
{
	out_color = texture(cubemap_sampler, frag_tex_coord);
}