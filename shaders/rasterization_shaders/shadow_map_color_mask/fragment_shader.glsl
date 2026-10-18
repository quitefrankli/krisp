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
layout(push_constant) uniform RasterizationPushConstantBuffer
{
	RasterizationPushConstant data;
} push_constant;

void main()
{
	const float effective_alpha = get_pbr_effective_alpha(
		mat_data.data.base_color_factor.a, push_constant.data.material);
	if (is_pbr_alpha_discarded(effective_alpha, push_constant.data.material))
		discard;
	const PointLightData light = global_data.data.lights[push_constant.data.light_index];
	gl_FragDepth = length(world_pos - light.position)
		/ global_data.data.shadow_far_plane;
}
