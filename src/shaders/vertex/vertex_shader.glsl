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
layout(binding = 0) uniform UniformBufferObject
{
	mat4 model;
	mat4 view;
	mat4 proj;
} ubo;

const vec3 light_dir = vec3(0.0, -1.0, 0.0);
const float minimum_lighting = 0.05;

void main()
{
	gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
	vec3 transformed_normal = vec3(ubo.model * vec4(inNormal, 1.0));
	// negative because we want opposing normals to be bright
	lighting = clamp(-dot(transformed_normal, light_dir), minimum_lighting, 1.0);
	fragTexCoord = inTexCoord;
}