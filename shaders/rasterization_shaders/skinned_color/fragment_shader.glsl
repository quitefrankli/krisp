#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "../../library/library.glsl"

layout(location=2) in vec3 surface_normal;
layout(location=4) in vec3 frag_pos;

layout(location=0) out vec4 out_color;

layout(set=RASTERIZATION_HIGH_FREQ_PER_SHAPE_SET_OFFSET, binding=RASTERIZATION_MATERIAL_DATA_BINDING) buffer MaterialDataBuffer
{
	MaterialData data;
} mat_data;

layout(set=RASTERIZATION_LOW_FREQ_SET_OFFSET, binding=RASTERIZATION_GLOBAL_DATA_BINDING) uniform GlobalDataBuffer
{
	GlobalData data;
} global_data;

layout(set=RASTERIZATION_SHADOW_MAP_SET_OFFSET, binding=RASTERIZATION_SHADOW_MAP_DATA_BINDING) uniform samplerCube shadow_map;

layout(push_constant) uniform AlphaMaterialBuffer
{
	AlphaMaterialData data;
} alpha_material;

float compute_shadow_factor(vec3 normal, vec3 light_dir)
{
	const vec3 frag_to_light = frag_pos - global_data.data.light_pos;
	return get_point_shadow_factor(
		shadow_map, frag_to_light, normal, light_dir, global_data.data.shadow_far_plane);
}

void main()
{
	const vec3 normal = normalize(surface_normal);
	const vec3 view_dir = normalize(global_data.data.view_pos - frag_pos);
	vec3 light_dir;
	const vec3 direct_light = evaluate_gltf_point_light(
		mat_data.data,
		normal,
		view_dir,
		frag_pos,
		global_data.data.light_pos,
		global_data.data.light_color,
		global_data.data.light_intensity,
		light_dir);
	const float alpha = mat_data.data.base_color_factor.a
		* alpha_material.data.opacity;
	out_color = vec4(
		direct_light * compute_shadow_factor(normal, light_dir), alpha);
}
