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

float compute_shadow_factor(vec3 normal, vec3 light_dir)
{
	const vec3 frag_to_light = frag_pos - global_data.data.light_pos;
	const float current_depth = length(frag_to_light);
	const float closest_depth = texture(shadow_map, frag_to_light).r * global_data.data.shadow_far_plane;
	const float bias = max(0.03 * (1.0 - dot(normal, light_dir)), 0.003);
	return (current_depth - bias) > closest_depth ? 0.05 : 1.0;
}

void main()
{
	const vec3 ambient = AMBIENT_STRENGTH * mat_data.data.ambient;
	const vec3 normal = normalize(surface_normal);
	const vec3 light_dir = normalize(global_data.data.light_pos - frag_pos);
	const float diffuse_factor = max(dot(normal, light_dir), 0.0);
	const vec3 diffuse = diffuse_factor * DIFFUSE_STRENGTH * global_data.data.lighting_scalar * mat_data.data.diffuse;
	const vec3 view_dir = normalize(global_data.data.view_pos - frag_pos);
	const float specular_factor = diffuse_factor > 0.0
		? get_bling_phong_spec(light_dir, normal, view_dir, mat_data.data.shininess) : 0.0;
	const vec3 specular = mat_data.data.specular
		* (SPECULAR_STRENGTH * specular_factor * global_data.data.lighting_scalar);
	const vec3 emissive = EMISSIVE_STRENGTH * mat_data.data.emissive;
	out_color = vec4(
		ambient + (diffuse + specular) * compute_shadow_factor(normal, light_dir) + emissive,
		1.0);
}
