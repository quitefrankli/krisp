#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "../../library/library.glsl"

// note that the fragment shader receives the input as interpolated values
layout(location=0) in vec2 frag_tex_coord;
layout(location=1) in vec3 surface_normal;
layout(location=2) in vec3 frag_pos;
layout(location=3) in vec4 surface_tangent;

layout(location = 0) out vec4 out_color;

const vec3 light_color = vec3(1.0, 1.0, 1.0);


layout(set=RASTERIZATION_HIGH_FREQ_PER_SHAPE_SET_OFFSET, binding=RASTERIZATION_ALBEDO_TEXTURE_DATA_BINDING) uniform sampler2D tex_sampler;
layout(set=RASTERIZATION_HIGH_FREQ_PER_SHAPE_SET_OFFSET, binding=RASTERIZATION_NORMAL_TEXTURE_DATA_BINDING) uniform sampler2D normal_sampler;
layout(set=RASTERIZATION_HIGH_FREQ_PER_SHAPE_SET_OFFSET, binding=RASTERIZATION_SPECULAR_TEXTURE_DATA_BINDING) uniform sampler2D specular_sampler;

layout(set=RASTERIZATION_LOW_FREQ_SET_OFFSET, binding=RASTERIZATION_GLOBAL_DATA_BINDING) uniform GlobalDataBuffer
{
	GlobalData data;
} global_data;

layout(set=RASTERIZATION_SHADOW_MAP_SET_OFFSET, binding=RASTERIZATION_SHADOW_MAP_DATA_BINDING) uniform samplerCube shadow_map;

layout(push_constant) uniform AlphaMaterialBuffer
{
	AlphaMaterialData data;
} alpha_material;

float compute_shadow_factor(const vec3 normal, const vec3 light_dir)
{
	const vec3 frag_to_light = frag_pos - global_data.data.light_pos;
	const float current_depth = length(frag_to_light);
	const float closest_depth = texture(shadow_map, frag_to_light).r * global_data.data.shadow_far_plane;
	const float bias = max(0.03 * (1.0 - dot(normal, light_dir)), 0.003);
	return (current_depth - bias) > closest_depth ? 0.05 : 1.0;
}

void main()
{
	const vec4 base_color = texture(tex_sampler, frag_tex_coord);
	const float alpha = base_color.a * alpha_material.data.opacity;
	if (alpha < alpha_material.data.alpha_cutoff)
		discard;
	vec3 color = base_color.rgb;

	// ambient
	vec3 ambient = color * 0.03;
	
    // diffuse 
	vec3 geometric_normal = normalize(surface_normal);
	vec3 tangent = normalize(surface_tangent.xyz - geometric_normal * dot(surface_tangent.xyz, geometric_normal));
	vec3 bitangent = cross(geometric_normal, tangent) * surface_tangent.w;
	vec3 tangent_normal = texture(normal_sampler, frag_tex_coord).xyz * 2.0 - 1.0;
	vec3 norm = normalize(mat3(tangent, bitangent, geometric_normal) * tangent_normal);
    vec3 lightDir = normalize(global_data.data.light_pos - frag_pos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * color * global_data.data.lighting_scalar;
    
    // specular
    vec3 viewDir = normalize(global_data.data.view_pos - frag_pos);
	const float default_specular_factor = 16.0;
	// in phong model, specular can have value on the opposite face
	// this is not good so we only emit specular is diffuse > 0
	const float spec = diff > 0.0 ? get_bling_phong_spec(lightDir, norm, viewDir, default_specular_factor) : 0.0;
	const vec4 specular_sample = texture(specular_sampler, frag_tex_coord);
	const vec3 specular = light_color * specular_sample.rgb * specular_sample.a
		* (SPECULAR_STRENGTH * global_data.data.lighting_scalar * spec);

	out_color = vec4(ambient + (diffuse + specular) * compute_shadow_factor(norm, lightDir), alpha);
}
