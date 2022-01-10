#version 450
#extension GL_ARB_separate_shader_objects : enable

// note that the fragment shader receives the input as interpolated values
layout(location=0) in vec2 fragTexCoord;
layout(location=1) in vec3 light_normal;
layout(location=2) in vec3 surface_normal;
layout(location=3) in vec3 view_dir;
layout(location=4) in vec3 fragPos;

layout(location = 0) out vec4 outColor;

const vec3 light_color = vec3(1.0, 1.0, 1.0);


layout(set=0, binding=1) uniform sampler2D texSampler;

layout(set=1, binding=0) uniform GlobalUniformBufferObject
{
	mat4 view; // camera
	mat4 proj;
	vec3 view_pos; // camera eye
	vec3 light_pos;
	float lighting_scalar;
} gubo;

void main()
{
	vec3 color = texture(texSampler, fragTexCoord).rgb; // note that we lose alpha channel here

	// ambient
	vec3 ambient = color * 0.03;
	
    // diffuse 
    vec3 norm = normalize(surface_normal);
    vec3 lightDir = normalize(gubo.light_pos - fragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * color * gubo.lighting_scalar;
    
    // specular
    float specularStrength = 0.5;
    vec3 viewDir = normalize(gubo.view_pos - fragPos);
    vec3 reflectDir = reflect(-lightDir, norm);  
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * light_color * gubo.lighting_scalar;

	outColor = vec4(ambient + diffuse + specular, 1.0);
}