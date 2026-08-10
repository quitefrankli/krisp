#version 450

#include "../../library/library.glsl"

layout(location = 0) out vec4 out_color;

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
	const float effective_alpha = get_pbr_effective_alpha(
		mat_data.data.base_color_factor.a, alpha_material.data);
	if (is_pbr_alpha_discarded(effective_alpha, alpha_material.data))
		discard;
	const float alpha = get_pbr_output_alpha(effective_alpha, alpha_material.data);
	out_color = vec4(mat_data.data.base_color_factor.rgb, alpha);
}
