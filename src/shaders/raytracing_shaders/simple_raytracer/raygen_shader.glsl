#version 460
#extension GL_EXT_ray_tracing : require


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

layout(set=2, binding=0) uniform accelerationStructureEXT topLevelAS;
layout(set=2, binding=1, rgba32f) uniform image2D image;

void main() 
{
    imageStore(image, ivec2(gl_LaunchIDEXT.xy), vec4(0.5, 0.5, 0.5, 1.0));
}
