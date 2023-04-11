#version 450
#extension GL_ARB_separate_shader_objects : enable

// note that the fragment shader receives the input as interpolated values
layout(location=0) in vec3 fragColor;
layout(location=4) in vec3 fragPos;

layout(location = 0) out vec4 outColor;

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
	outColor = vec4(fragColor, 1.0);
}