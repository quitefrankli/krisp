#include "../../shared_code/shared_data_structures.glsl"

const float PI = 3.14159265358979323846;
const float MIN_PERCEPTUAL_ROUGHNESS = 0.04;
const float MIN_LIGHT_DISTANCE_SQUARED = 0.0001;
const int PBR_ALPHA_MODE_OPAQUE = 0;
const int PBR_ALPHA_MODE_MASK = 1;
const int PBR_ALPHA_MODE_BLEND = 2;

vec4 sample_pbr_base_color(
	const MaterialData material,
	sampler2D base_color_sampler,
	const vec2 tex_coord,
	const int premultiplied_base_color)
{
	vec4 base_color = material.base_color_factor;
	if ((material.texture_flags & PBR_BASE_COLOR_TEXTURE) == 0)
		return base_color;

	vec4 sampled_color = texture(base_color_sampler, tex_coord);
	if (premultiplied_base_color != 0)
		sampled_color.rgb = sampled_color.a > 0.0
			? sampled_color.rgb / sampled_color.a : vec3(0.0);
	return base_color * sampled_color;
}

float get_pbr_effective_alpha(
	const float base_color_alpha,
	const AlphaMaterialData alpha_material)
{
	return base_color_alpha * alpha_material.opacity;
}

bool is_pbr_alpha_discarded(
	const float effective_alpha,
	const AlphaMaterialData alpha_material)
{
	return alpha_material.alpha_mode == PBR_ALPHA_MODE_MASK
		&& effective_alpha < alpha_material.alpha_cutoff;
}

float get_pbr_output_alpha(
	const float effective_alpha,
	const AlphaMaterialData alpha_material)
{
	return alpha_material.alpha_mode == PBR_ALPHA_MODE_BLEND
		? effective_alpha : 1.0;
}

vec3 orient_pbr_normal(
	const vec3 normal,
	const int double_sided,
	const bool front_facing)
{
	return double_sided != 0 && !front_facing ? -normal : normal;
}

vec3 get_pbr_emissive(
	const MaterialData material,
	sampler2D emissive_sampler,
	const vec2 tex_coord)
{
	vec3 emissive = material.emissive_factor;
	if ((material.texture_flags & PBR_EMISSIVE_TEXTURE) != 0)
		emissive *= texture(emissive_sampler, tex_coord).rgb;
	return emissive;
}

vec3 get_direction_to_point_light(
	const vec3 fragment_position,
	const vec3 light_position)
{
	const vec3 to_light = light_position - fragment_position;
	return to_light * inversesqrt(max(
		dot(to_light, to_light), MIN_LIGHT_DISTANCE_SQUARED));
}


// Khronos glTF 2.0 metallic-roughness BRDF. The returned value includes NdotL,
// but not incident radiance or shadow visibility.
vec3 evaluate_gltf_metallic_roughness_brdf(
	const MaterialData material,
	const vec3 normal,
	const vec3 view_dir,
	const vec3 light_dir)
{
	const float metallic = clamp(material.metallic_factor, 0.0, 1.0);
	const float perceptual_roughness = clamp(
		material.roughness_factor, MIN_PERCEPTUAL_ROUGHNESS, 1.0);
	const float alpha_roughness = perceptual_roughness * perceptual_roughness;
	const float alpha_squared = alpha_roughness * alpha_roughness;
	const vec3 base_color = material.base_color_factor.rgb;

	const float n_dot_l = clamp(dot(normal, light_dir), 0.0, 1.0);
	const float n_dot_v = clamp(dot(normal, view_dir), 0.0, 1.0);
	const vec3 half_sum = light_dir + view_dir;
	const vec3 half_vector = half_sum * inversesqrt(max(
		dot(half_sum, half_sum), 0.000001));
	const float n_dot_h = clamp(dot(normal, half_vector), 0.0, 1.0);
	const float v_dot_h = clamp(dot(view_dir, half_vector), 0.0, 1.0);

	const vec3 f0 = mix(vec3(0.04), base_color, metallic);
	const vec3 fresnel = f0 + (vec3(1.0) - f0) * pow(1.0 - v_dot_h, 5.0);

	const float distribution_denominator =
		n_dot_h * n_dot_h * (alpha_squared - 1.0) + 1.0;
	const float distribution = alpha_squared
		/ (PI * distribution_denominator * distribution_denominator);

	// Height-correlated Smith masking-shadowing from the glTF reference BRDF.
	const float ggx_view = n_dot_l * sqrt(
		n_dot_v * n_dot_v * (1.0 - alpha_squared) + alpha_squared);
	const float ggx_light = n_dot_v * sqrt(
		n_dot_l * n_dot_l * (1.0 - alpha_squared) + alpha_squared);
	const float visibility = 0.5 / max(ggx_view + ggx_light, 0.000001);

	const vec3 diffuse = (vec3(1.0) - fresnel)
		* (1.0 - metallic) * base_color / PI;
	const vec3 specular = fresnel * visibility * distribution;
	return (diffuse + specular) * n_dot_l;
}

vec3 evaluate_gltf_point_light(
	const MaterialData material,
	const vec3 normal,
	const vec3 view_dir,
	const vec3 fragment_position,
	const vec3 light_position,
	const vec3 light_color,
	const float light_intensity,
	out vec3 light_dir)
{
	const vec3 to_light = light_position - fragment_position;
	const float distance_squared = max(
		dot(to_light, to_light), MIN_LIGHT_DISTANCE_SQUARED);
	light_dir = get_direction_to_point_light(
		fragment_position, light_position);
	const vec3 incident_radiance = light_color
		* max(light_intensity, 0.0) / distance_squared;
	return evaluate_gltf_metallic_roughness_brdf(
		material, normal, view_dir, light_dir) * incident_radiance;
}


float get_phong_spec(vec3 lightDir, vec3 norm, vec3 viewDir, float shininess)
{
    const vec3 reflectDir = reflect(-lightDir, norm);

    return pow(max(dot(viewDir, reflectDir), 0.0), shininess);
}

float get_bling_phong_spec(vec3 lightDir, vec3 norm, vec3 viewDir, float shininess)
{
	const vec3 halfway_dir = normalize(lightDir + viewDir);
	const float specular_compensation = 2.0; // using halfway dir means strength is roughly halfed

	return pow(max(dot(norm, halfway_dir), 0.0), shininess * specular_compensation);
}

float get_point_shadow_factor(
	samplerCube shadow_map,
	const vec3 frag_to_light,
	const vec3 normal,
	const vec3 light_dir,
	const float shadow_far_plane)
{
	const float current_depth = length(frag_to_light);
	if (current_depth <= 0.000001)
		return 1.0;
	const float bias = max(0.03 * (1.0 - dot(normal, light_dir)), 0.003);
	const vec3 lookup_dir = normalize(frag_to_light);

	// Build a stable tangent frame around the cubemap lookup direction so the
	// two-dimensional filter kernel can sample across the cubemap surface.
	const vec3 reference_axis = abs(lookup_dir.y) < 0.999
		? vec3(0.0, 1.0, 0.0)
		: vec3(1.0, 0.0, 0.0);
	const vec3 tangent = normalize(cross(reference_axis, lookup_dir));
	const vec3 bitangent = cross(lookup_dir, tangent);
	const float texel_size = 2.0 / float(textureSize(shadow_map, 0).x);

	// Irregular spacing avoids the grid-shaped edges produced by box PCF.
	// Keep the sample count and visibility divisor in sync.
	const vec2 poisson_disk[16] = vec2[](
		vec2(-0.94201624, -0.39906216),
		vec2(0.94558609, -0.76890725),
		vec2(-0.09418410, -0.92938870),
		vec2(0.34495938, 0.29387760),
		vec2(-0.91588581, 0.45771432),
		vec2(-0.81544232, -0.87912464),
		vec2(-0.38277543, 0.27676845),
		vec2(0.97484398, 0.75648379),
		vec2(0.44323325, -0.97511554),
		vec2(0.53742981, -0.47373420),
		vec2(-0.26496911, -0.41893023),
		vec2(0.79197514, 0.19090188),
		vec2(-0.24188840, 0.99706507),
		vec2(-0.81409955, 0.91437590),
		vec2(0.19984126, 0.78641367),
		vec2(0.14383161, -0.14100790));

	float visibility = 0.0;
	for (int sample_idx = 0; sample_idx < 16; ++sample_idx)
	{
		const vec2 offset = poisson_disk[sample_idx] * texel_size;
		const vec3 sample_dir = normalize(
			lookup_dir + tangent * offset.x + bitangent * offset.y);

		// Project this tap onto the receiver's local plane. Comparing every tap
		// with current_depth causes concentric self-shadowing on large planes
		// because their cubemap depth changes across the filter footprint.
		const float receiver_plane_distance = dot(frag_to_light, normal);
		const float sample_plane_angle = dot(sample_dir, normal);
		const float sample_depth = abs(sample_plane_angle) > 0.0001
			? receiver_plane_distance / sample_plane_angle
			: current_depth;
		const float closest_depth = texture(shadow_map, sample_dir).r * shadow_far_plane;
		visibility += (sample_depth - bias) > closest_depth ? 0.0 : 1.0;
	}

	return visibility / 16.0;
}
