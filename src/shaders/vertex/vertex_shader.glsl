#version 450

// keep in mind that some types such as dvec3 uses 2 slots therefore we need the next layout location to be 2 indices after
layout(location = 0) in vec3 inPosition;
// layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;

// layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out float lighting;
// be vary of alignment issues
layout(set=0, binding=0) uniform UniformBufferObject
{
	mat4 model;
	mat4 view;
	mat4 proj;
} ubo;

layout(set=1, binding=0) uniform GlobalUniformBufferObject
{
	vec3 light_pos;
	float lighting_scalar;
} gubo;

const float minimum_lighting = 0.1;

vec3 get_light_normal(mat3 m)
{
	return normalize(gubo.light_pos - m * inPosition);
}

void main()
{
	gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
	vec3 transformed_normal = mat3(ubo.model) * inNormal;
	lighting = clamp(dot(transformed_normal, get_light_normal(mat3(ubo.model))) * gubo.lighting_scalar, minimum_lighting, 1.0);
	fragTexCoord = inTexCoord;
}