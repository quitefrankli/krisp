#version 450
#extension GL_GOOGLE_include_directive : enable

#include "../../library/library.glsl"

layout(location=0) in vec3 world_pos;
layout(location=1) in vec2 tex_coord;

layout(set=RASTERIZATION_HIGH_FREQ_PER_SHAPE_SET_OFFSET, binding=RASTERIZATION_ALBEDO_TEXTURE_DATA_BINDING) uniform sampler2D tex_sampler;
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
	float base_alpha = mat_data.data.base_color_factor.a;
	if ((mat_data.data.texture_flags & PBR_BASE_COLOR_TEXTURE) != 0)
		base_alpha *= texture(tex_sampler, tex_coord).a;
	const float effective_alpha = get_pbr_effective_alpha(base_alpha, alpha_material.data);
	if (is_pbr_alpha_discarded(effective_alpha, alpha_material.data))
		discard;
	const float light_distance = length(world_pos - global_data.data.light_pos);
	gl_FragDepth = light_distance / global_data.data.shadow_far_plane;
}
