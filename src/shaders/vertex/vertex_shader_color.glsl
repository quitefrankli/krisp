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
	mat4 mvp; // precomputed model-view-proj matrix
	mat3 rot_mat; // in c++ this is actually a mat4
} ubo;

layout(set=1, binding=0) uniform GlobalUniformBufferObject
{
	mat4 view; // camera
	mat4 proj;
	vec3 light_pos;
	float lighting_scalar;
} gubo;

const float minimum_lighting = 0.02;

vec3 get_light_normal()
{
	return normalize(gubo.light_pos - ubo.rot_mat * inPosition);
}

void main()
{
	gl_Position = ubo.mvp * vec4(inPosition, 1.0);
	vec3 transformed_normal = ubo.rot_mat * inNormal;
	fragColor = inColor * clamp(dot(transformed_normal, get_light_normal()) * gubo.lighting_scalar, minimum_lighting, 1.0);
}