#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "../../library/library.glsl"

layout(set = 0, binding = 0) uniform sampler2D hdr_scene;
layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 out_color;

layout(push_constant) uniform PushConstant
{
	PresentationPushConstant data;
} push_constant;

// Stephen Hill's fit of the ACES reference curve, popularised by Krzysztof Narkowicz.
vec3 aces_fitted(vec3 color)
{
	const float a = 2.51;
	const float b = 0.03;
	const float c = 2.43;
	const float d = 0.59;
	const float e = 0.14;
	return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

void main()
{
	vec3 scene_color = max(texture(hdr_scene, frag_tex_coord).rgb, vec3(0.0));
	scene_color *= exp2(push_constant.data.exposure_ev);
	out_color = vec4(aces_fitted(scene_color), 1.0);
}
