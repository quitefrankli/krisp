#version 450

// keep in mind that some types such as dvec3 uses 2 slots therefore we need the next layout location to be 2 indices after
layout(location = 0) in vec3 inPosition;
// layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;

layout(location=0) out vec2 fragTexCoord;
layout(location=1) out vec3 light_normal;
layout(location=2) out vec3 surface_normal;
layout(location=3) out vec3 view_dir;
layout(location=4) out vec3 fragPos;

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
	vec3 view_pos; // camera eye
	vec3 light_pos;
	float lighting_scalar;
} gubo;

vec3 get_light_normal()
{
	return normalize(gubo.light_pos - ubo.rot_mat * inPosition);
}

void main()
{
	gl_Position = ubo.mvp * vec4(inPosition, 1.0);

	fragTexCoord = inTexCoord;
	light_normal = get_light_normal();
	surface_normal = ubo.rot_mat * inNormal;
	// it's likely we can remove the need for gubo.view_pos and compute everything in "view space"
	view_dir = normalize(gubo.view_pos - mat3(ubo.model) * inPosition);
	fragPos = (ubo.model * vec4(inPosition, 1.0)).xyz;
}