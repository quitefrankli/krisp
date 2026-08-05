#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "../../library/library.glsl"

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 1) in vec4 frag_color;

layout(location = 0) out vec4 out_color;

void main()
{
	vec4 color = frag_color;
	vec2 center = frag_tex_coord - vec2(0.5);
	float dist = length(center) * 2.0;
	float alpha = 1.0 - smoothstep(0.8, 1.0, dist);
	color.a *= alpha;

	if (color.a < 0.01)
	{
		discard;
	}

	out_color = color;
}
