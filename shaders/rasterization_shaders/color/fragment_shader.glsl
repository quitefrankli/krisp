#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "../../../shared_code/shared_data_structures.glsl"

// note that the fragment shader receives the input as interpolated values
layout(location=2) in vec3 surface_normal;
layout(location=4) in vec3 fragPos;

layout(location = 0) out vec4 outColor;

const vec3 light_color = vec3(1.0, 1.0, 1.0);

layout(set=0, binding=2) buffer MaterialDataBuffer
{
	MaterialData data;
} mat_data;

layout(set=1, binding=0) uniform GlobalDataBuffer
{
	GlobalData data;
} global_data;

void main()
{
	// ambient
	const float ambientStrength = 0.03;
	const vec3 ambient = mat_data.data.ambient * ambientStrength;
	
    // diffuse 
    const vec3 norm = normalize(surface_normal);
    const vec3 lightDir = normalize(global_data.data.light_pos - fragPos);
    const float diff = max(dot(norm, lightDir), 0.0);
    const vec3 diffuse = diff * global_data.data.lighting_scalar * mat_data.data.diffuse;
    
    // specular
    const vec3 viewDir = normalize(global_data.data.view_pos - fragPos);
    const vec3 reflectDir = reflect(-lightDir, norm);  
    const float spec = pow(max(dot(viewDir, reflectDir), 0.0), mat_data.data.shininess);
    const float specularStrength = 0.5;
    const vec3 specular = specularStrength * spec * global_data.data.lighting_scalar * mat_data.data.specular;
        
	outColor = vec4(ambient + diffuse + specular, 1.0);
}