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