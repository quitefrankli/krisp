#version 460
#extension GL_EXT_ray_tracing : require

#include "../../library/library.glsl"
#include "../raytracing_common.glsl"

// be vary of alignment issues
layout(set=RAYTRACING_TLAS_SET_OFFSET, binding=RAYTRACING_TLAS_DATA_BINDING) uniform accelerationStructureEXT topLevelAS;
layout(set=RAYTRACING_TLAS_SET_OFFSET, binding=RAYTRACING_ALBEDO_TEXTURE_DATA_BINDING, rgba32f) uniform image2D image;

layout(set=RAYTRACING_LOW_FREQ_SET_OFFSET, binding=RAYTRACING_GLOBAL_DATA_BINDING) uniform GlobalDataBuffer
{
	GlobalData data;
} global_data;

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

	// vec4 origin = vec4(global_data.data.view_pos, 1);
	vec4 target = inverse(global_data.data.proj) * vec4(pixel_normalized, 1, 1);
	vec4 direction = inverse(global_data.data.view) * vec4(target.xyz, 0);

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
		global_data.data.view_pos,  // ray origin
		tMin,           // ray min range
		direction.xyz,      // ray direction
		tMax,           // ray max range
		0);             // payload (location = 0)

	// 'imageStore' stores a vec4 value to an image
	const uint yflipped = gl_LaunchSizeEXT.y - gl_LaunchIDEXT.y - 1;
    imageStore(image, ivec2(gl_LaunchIDEXT.x, yflipped), vec4(payload.hit_value, 1.0));
}