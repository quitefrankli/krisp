#version 450

#include "../../library/library.glsl"

layout(set=0, binding=0) uniform sampler2D source_texture;
layout(location=0) in vec2 frag_tex_coord;
layout(location=0) out vec4 out_color;

layout(push_constant) uniform PushConstant
{
	TextureCompositorPushConstant data;
} push_constant;

void main()
{
	const vec2 centre = push_constant.data.placement.xy;
	const vec2 scale = push_constant.data.placement.zw;
	const float angle = -push_constant.data.rotation_radians;
	const float c = cos(angle);
	const float s = sin(angle);
	const vec2 relative = frag_tex_coord - centre;
	const vec2 unrotated = mat2(c, -s, s, c) * relative;
	const vec2 source_uv = unrotated / scale + vec2(0.5);
	if (any(lessThan(source_uv, vec2(0.0))) || any(greaterThan(source_uv, vec2(1.0))))
	{
		out_color = vec4(0.0);
		return;
	}

	const vec4 sampled = texture(source_texture, source_uv);
	const float alpha = sampled.a * push_constant.data.tint_opacity.a;
	out_color = vec4(sampled.rgb * push_constant.data.tint_opacity.rgb * alpha, alpha);
}
