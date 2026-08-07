#version 450

#include "../../library/library.glsl"

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 out_color;

layout(set=RASTERIZATION_HIGH_FREQ_PER_SHAPE_SET_OFFSET, binding=RASTERIZATION_ALBEDO_TEXTURE_DATA_BINDING) uniform sampler2D base_color_sampler;

layout(set=RASTERIZATION_HIGH_FREQ_PER_SHAPE_SET_OFFSET, binding=RASTERIZATION_MATERIAL_DATA_BINDING) buffer MaterialDataBuffer
{
	MaterialData data;
} mat_data;

layout(push_constant) uniform AlphaMaterialBuffer
{
	AlphaMaterialData data;
} alpha_material;

void main()
{
	vec4 base_color = mat_data.data.base_color_factor;
	if ((mat_data.data.texture_flags & PBR_BASE_COLOR_TEXTURE) != 0)
	{
		vec4 sampled_color = texture(base_color_sampler, frag_tex_coord);
		if (alpha_material.data.premultiplied_base_color != 0)
			sampled_color.rgb = sampled_color.a > 0.0
				? sampled_color.rgb / sampled_color.a : vec3(0.0);
		base_color *= sampled_color;
	}
	const float alpha = base_color.a * alpha_material.data.opacity;
	if (alpha < alpha_material.data.alpha_cutoff)
		discard;
	out_color = vec4(base_color.rgb, alpha);
}
