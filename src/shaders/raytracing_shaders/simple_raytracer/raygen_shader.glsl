#version 460
#extension GL_EXT_ray_tracing : require

#include "../raytracing_common.glsl"

// be vary of alignment issues
layout(set=0, binding=0) uniform accelerationStructureEXT topLevelAS;
layout(set=0, binding=1, rgba32f) uniform image2D image;

layout(set=1, binding=0) uniform GlobalUniformBufferObject
{
	mat4 view; // camera
	mat4 proj;
	vec3 view_pos; // camera eye (not focus object)
	vec3 light_pos;
	float lighting_scalar;
} gubo;

layout(location = 0) rayPayloadEXT HitPayload payload;

void main() 
{
	// 'gl_LaunchIDEXT' contains the integer coordinates of the pixel being rendererd
	// for standard ray tracing variables and functions see 
	// https://github.com/KhronosGroup/GLSL/blob/master/extensions/ext/GLSL_EXT_ray_tracing.txt
	const vec2 pixel_center = vec2(gl_LaunchIDEXT.xy) + vec2(0.5);
	// 'gl_LaunchSizeEXT' contains the size of the image being rendered
	const vec2 inUV = pixel_center / vec2(gl_LaunchSizeEXT.xy);
	vec2 pixel_normalized = inUV * 2.0 - 1.0; // normalize to [-1, 1]

	// vec4 origin = vec4(gubo.view_pos, 1);
	vec4 target = inverse(gubo.view) * vec4(pixel_normalized, 1, 1);
	vec3 direction = normalize(target.xyz - gubo.view_pos);

	uint  rayFlags = gl_RayFlagsOpaqueEXT;
	float tMin     = 0.001;
	float tMax     = 10000.0;

	traceRayEXT(
		topLevelAS, 	// tlas
		rayFlags,       // rayFlags
		0xFF,           // cullMask
		0,              // sbtRecordOffset
		0,              // sbtRecordStride
		0,              // missIndex
		gubo.view_pos,  // ray origin
		tMin,           // ray min range
		direction,      // ray direction
		tMax,           // ray max range
		0);             // payload (location = 0)

	// 'imageStore' stores a vec4 value to an image
    imageStore(image, ivec2(gl_LaunchIDEXT.xy), vec4(payload.hit_value, 1.0));
}