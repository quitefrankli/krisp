#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "../../library/library.glsl"

// note that the fragment shader receives the input as interpolated values
layout(location=2) in vec3 surface_normal;
layout(location=4) in vec3 frag_pos;
layout(location=5) in vec4 shadow_coords;

layout(location=0) out vec4 out_color;

layout(set=RASTERIZATION_HIGH_FREQ_PER_SHAPE_SET_OFFSET, binding=RASTERIZATION_MATERIAL_DATA_BINDING) buffer MaterialDataBuffer
{
	MaterialData data;
} mat_data;

layout(set=RASTERIZATION_LOW_FREQ_SET_OFFSET, binding=RASTERIZATION_GLOBAL_DATA_BINDING) uniform GlobalDataBuffer
{
	GlobalData data;
} global_data;

layout(set=RASTERIZATION_SHADOW_MAP_SET_OFFSET, binding=RASTERIZATION_SHADOW_MAP_DATA_BINDING) uniform sampler2D shadow_map;

float compute_shadow_factor(vec4 shadow_coords)
{
	// if (abs(shadow_coords.z) > 1.0)
	// {
	// 	// outside NDC, so treat as if not in shadow
	// 	return 1.0;
	// }

	// convert light space to screen space NDC
	shadow_coords.xyz /= shadow_coords.w;
	// Vulkan's Z is already in NDC [0 : 1]
	shadow_coords.xy = shadow_coords.xy * 0.5 + 0.5;

	shadow_coords.y = 1.0 - shadow_coords.y; // I'm not sure why it appears the y-axis is flipped, adding here

	float closest_depth = texture(shadow_map, shadow_coords.xy).r; 

	// check whether current frag pos is in shadow
	return shadow_coords.z > closest_depth ? 0.05 : 1.0;
}

void main()
{
	// ambient
	const vec3 ambient = AMBIENT_STRENGTH * mat_data.data.ambient;
	
    // diffuse 
    const vec3 norm = normalize(surface_normal);
    const vec3 lightDir = normalize(global_data.data.light_pos - frag_pos);
    const float diff = max(dot(norm, lightDir), 0.0);
    const vec3 diffuse = diff * DIFFUSE_STRENGTH * global_data.data.lighting_scalar * mat_data.data.diffuse;
    
    // specular
    const vec3 viewDir = normalize(global_data.data.view_pos - frag_pos);
	// in phong model, specular can have value on the opposite face
	// this is not good so we only emit specular is diffuse > 0
	const float spec = diff > 0.0 ? get_bling_phong_spec(lightDir, norm, viewDir, mat_data.data.shininess) : 0.0;
    const vec3 specular = mat_data.data.specular * (SPECULAR_STRENGTH * spec * global_data.data.lighting_scalar);

	// emissive
	const vec3 emissive = EMISSIVE_STRENGTH * mat_data.data.emissive;
        
	out_color = vec4(ambient + (diffuse + specular)*compute_shadow_factor(shadow_coords) + emissive, 1.0);
}