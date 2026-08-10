#version 450

#include "../../library/library.glsl"

layout(location=0) in vec2 tex_coord;
layout(location=0) out vec4 out_color;
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
	float base_alpha = mat_data.data.base_color_factor.a;
	if ((mat_data.data.texture_flags & PBR_BASE_COLOR_TEXTURE) != 0)
		base_alpha *= texture(base_color_sampler, tex_coord).a;
	const float effective_alpha = get_pbr_effective_alpha(base_alpha, alpha_material.data);
	if (is_pbr_alpha_discarded(effective_alpha, alpha_material.data))
		discard;
	out_color = STENCIL_COLOR;
}
