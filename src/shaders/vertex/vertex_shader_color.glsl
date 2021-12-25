#version 450

// keep in mind that some types such as dvec3 uses 2 slots therefore we need the next layout location to be 2 indices after
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 3) in vec3 inNormal;

layout(location = 0) out vec3 fragColor;

// be vary of alignment issues
layout(set=0, binding=0) uniform UniformBufferObject
{
	mat4 model;
	mat4 view;
	mat4 proj;
} ubo;

layout(set=1, binding=0) uniform GlobalUniformBufferObject
{
	float lighting_scalar;
} gubo;

const vec3 light_dir = vec3(0.0, -1.0, 0.0);
const float minimum_lighting = 0.2;

void main()
{
	gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
	vec3 transformed_normal = vec3(ubo.model * vec4(inNormal, 1.0));
	// negative because we want opposing normals to be bright
	fragColor = inColor * clamp(-dot(transformed_normal, light_dir) * gubo.lighting_scalar, minimum_lighting, 1.0);
}