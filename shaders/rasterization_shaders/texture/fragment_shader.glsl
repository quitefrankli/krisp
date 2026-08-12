#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "../../library/library.glsl"

// note that the fragment shader receives the input as interpolated values
layout(location=0) in vec2 frag_tex_coord;
layout(location=1) in vec3 surface_normal;
layout(location=2) in vec3 frag_pos;
layout(location=3) in vec4 surface_tangent;

layout(location = 0) out vec4 out_color;

layout(set=RASTERIZATION_HIGH_FREQ_PER_SHAPE_SET_OFFSET, binding=RASTERIZATION_ALBEDO_TEXTURE_DATA_BINDING) uniform sampler2D base_color_sampler;
layout(set=RASTERIZATION_HIGH_FREQ_PER_SHAPE_SET_OFFSET, binding=RASTERIZATION_NORMAL_TEXTURE_DATA_BINDING) uniform sampler2D normal_sampler;
layout(set=RASTERIZATION_HIGH_FREQ_PER_SHAPE_SET_OFFSET, binding=RASTERIZATION_METALLIC_ROUGHNESS_TEXTURE_DATA_BINDING) uniform sampler2D metallic_roughness_sampler;
layout(set=RASTERIZATION_HIGH_FREQ_PER_SHAPE_SET_OFFSET, binding=RASTERIZATION_EMISSIVE_TEXTURE_DATA_BINDING) uniform sampler2D emissive_sampler;

layout(set=RASTERIZATION_HIGH_FREQ_PER_SHAPE_SET_OFFSET, binding=RASTERIZATION_MATERIAL_DATA_BINDING) buffer MaterialDataBuffer
{
	MaterialData data;
} mat_data;

layout(set=RASTERIZATION_LOW_FREQ_SET_OFFSET, binding=RASTERIZATION_GLOBAL_DATA_BINDING) uniform GlobalDataBuffer
{
	GlobalData data;
} global_data;

layout(set=RASTERIZATION_LOW_FREQ_SET_OFFSET,
	binding=RASTERIZATION_IRRADIANCE_MAP_DATA_BINDING) uniform samplerCube irradiance_map;
layout(set=RASTERIZATION_LOW_FREQ_SET_OFFSET,
	binding=RASTERIZATION_PREFILTERED_ENVIRONMENT_DATA_BINDING) uniform samplerCube prefiltered_environment_map;
layout(set=RASTERIZATION_LOW_FREQ_SET_OFFSET,
	binding=RASTERIZATION_BRDF_LUT_DATA_BINDING) uniform sampler2D brdf_lut;

layout(set=RASTERIZATION_SHADOW_MAP_SET_OFFSET, binding=RASTERIZATION_SHADOW_MAP_DATA_BINDING) uniform samplerCube shadow_map;

layout(push_constant) uniform AlphaMaterialBuffer
{
	AlphaMaterialData data;
} alpha_material;

float compute_shadow_factor(const vec3 normal, const vec3 light_dir)
{
	const vec3 frag_to_light = frag_pos - global_data.data.light_pos;
	return get_point_shadow_factor(
		shadow_map, frag_to_light, normal, light_dir, global_data.data.shadow_far_plane);
}

void main()
{
	MaterialData material = mat_data.data;
	material.base_color_factor = sample_pbr_base_color(
		material, base_color_sampler, frag_tex_coord,
		alpha_material.data.premultiplied_base_color);
	if ((material.texture_flags & PBR_METALLIC_ROUGHNESS_TEXTURE) != 0)
	{
		const vec4 sample_value = texture(metallic_roughness_sampler, frag_tex_coord);
		material.roughness_factor *= sample_value.g;
		material.metallic_factor *= sample_value.b;
	}

	vec3 geometric_normal = normalize(surface_normal);
	vec3 shading_normal = geometric_normal;
	if ((material.texture_flags & PBR_NORMAL_TEXTURE) != 0)
	{
		const vec3 tangent = normalize(surface_tangent.xyz
			- geometric_normal * dot(surface_tangent.xyz, geometric_normal));
		const vec3 bitangent = cross(geometric_normal, tangent) * surface_tangent.w;
		vec3 tangent_normal = texture(normal_sampler, frag_tex_coord).xyz * 2.0 - 1.0;
		tangent_normal.xy *= material.normal_scale;
		shading_normal = normalize(mat3(tangent, bitangent, geometric_normal) * tangent_normal);
	}
	geometric_normal = orient_pbr_normal(
		geometric_normal, alpha_material.data.double_sided, gl_FrontFacing);
	shading_normal = orient_pbr_normal(
		shading_normal, alpha_material.data.double_sided, gl_FrontFacing);

	const vec3 view_dir = normalize(global_data.data.view_pos - frag_pos);
	vec3 light_dir;
	const vec3 direct_light = evaluate_gltf_point_light(
		material,
		shading_normal,
		view_dir,
		frag_pos,
		global_data.data.light_pos,
		global_data.data.light_color,
		global_data.data.light_intensity,
		light_dir);
	const float effective_alpha = get_pbr_effective_alpha(
		material.base_color_factor.a, alpha_material.data);
	if (is_pbr_alpha_discarded(effective_alpha, alpha_material.data))
		discard;
	const float alpha = get_pbr_output_alpha(effective_alpha, alpha_material.data);
	const vec3 emissive = get_pbr_emissive(
		material, emissive_sampler, frag_tex_coord);
	const vec3 environment_light = evaluate_gltf_image_based_lighting(
		material,
		shading_normal,
		view_dir,
		irradiance_map,
		prefiltered_environment_map,
		brdf_lut);
	out_color = vec4(
		direct_light * compute_shadow_factor(geometric_normal, light_dir)
			+ environment_light + emissive,
		alpha);
}
