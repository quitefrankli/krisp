#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "../../library/library.glsl"

layout(location=0) in vec3 world_pos;

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
	const PointLightData light = global_data.data.lights[push_constant.data.light_index];
	const float light_distance = length(world_pos - light.position);
	gl_FragDepth = light_distance / global_data.data.shadow_far_plane;
}
