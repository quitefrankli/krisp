#include "../../shared_code/shared_data_structures.glsl"


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

	// Preserve a small amount of direct light in fully shadowed regions.
	return mix(0.05, 1.0, visibility / 16.0);
}
