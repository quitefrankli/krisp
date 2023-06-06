#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "../../library/library.glsl"

layout(set=0, binding=0) uniform sampler2D tex_sampler;

layout(location=0) in vec2 frag_tex_coord;

layout(push_constant) uniform PushConstant
{
	QuadRendererPushConstant data;
} push_constant;

layout(location = 0) out vec4 out_color;

float linearize_depth(float depth)
{
	float n = 0.1;
	// for visualisation purposes, this value isn't the actual far plane but improves contrast
	float f = 100.0; 

	return (2.0 * n) / (f + n - depth * (f - n));
	// return 1.0 - (1.0 - depth) * 100.0;
	// return n * f / (f + depth * (n - f));
}

void main()
{
	switch (push_constant.data.flags)
	{
	case 1:
		float depth = texture(tex_sampler, frag_tex_coord).r;
		depth = linearize_depth(depth);
		out_color = vec4(vec3(depth), 1.0);
		break;
	default:
		out_color = texture(tex_sampler, frag_tex_coord);
		break;
	}
}