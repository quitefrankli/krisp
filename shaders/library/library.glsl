#include "../../shared_code/shared_data_structures.glsl"

const float AMBIENT_STRENGTH = 0.03;
const float DIFFUSE_STRENGTH = 0.9;
const float SPECULAR_STRENGTH = 0.5;
const float EMISSIVE_STRENGTH = 1.0; // it's high because for now only light sources use this value

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