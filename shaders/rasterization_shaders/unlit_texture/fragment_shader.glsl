#version 450

#include "../../library/library.glsl"

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 out_color;

layout(set=RASTERIZATION_HIGH_FREQ_PER_SHAPE_SET_OFFSET, binding=RASTERIZATION_ALBEDO_TEXTURE_DATA_BINDING) uniform sampler2D tex_sampler;

layout(push_constant) uniform AlphaMaterialBuffer
{
	AlphaMaterialData data;
} alpha_material;

void main()
{
	const vec4 base_color = texture(tex_sampler, frag_tex_coord);
	const float alpha = base_color.a * alpha_material.data.opacity;
	if (alpha < alpha_material.data.alpha_cutoff)
		discard;
	out_color = vec4(base_color.rgb, alpha);
}
