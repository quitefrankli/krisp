#version 450

#include "../../library/library.glsl"

layout(location=0) in vec3 world_pos;
layout(set=RASTERIZATION_HIGH_FREQ_PER_SHAPE_SET_OFFSET, binding=RASTERIZATION_MATERIAL_DATA_BINDING) buffer MaterialDataBuffer
{
	MaterialData data;
} mat_data;
layout(set=RASTERIZATION_LOW_FREQ_SET_OFFSET, binding=RASTERIZATION_GLOBAL_DATA_BINDING) uniform GlobalDataBuffer
{
	GlobalData data;
} global_data;
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
	gl_FragDepth = length(world_pos - global_data.data.light_pos)
		/ global_data.data.shadow_far_plane;
}
