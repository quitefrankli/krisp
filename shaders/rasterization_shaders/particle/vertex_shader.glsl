#version 450

#include "../../library/library.glsl"

// Vertex attributes (unit quad)
layout(location = 0) in vec2 in_local_pos;  // Local position (-0.5 to 0.5)
layout(location = 1) in vec2 in_tex_coord; // Texture coordinates

// Instance attributes
layout(location = 2) in vec3 in_world_pos;  // World position of particle
layout(location = 3) in float in_size;      // Particle size
layout(location = 4) in vec4 in_color;      // Particle color
layout(location = 5) in float in_rotation;  // Rotation angle

layout(location = 0) out vec2 frag_tex_coord;
layout(location = 1) out vec4 frag_color;

layout(set = RASTERIZATION_LOW_FREQ_SET_OFFSET, 
	   binding = RASTERIZATION_GLOBAL_DATA_BINDING) uniform GlobalDataBuffer
{
	GlobalData data;
} global_data;

// Billboard technique: constructs a rotation matrix that always faces the camera
// The right and up vectors are extracted from the view matrix to ensure the quad
// always faces the camera (billboard effect)
void main()
{
	// Get the camera's right and up vectors from the view matrix
	// The view matrix transforms world space to camera space
	// Its inverse (transpose for rotation) gives us camera axes in world space
	vec3 camera_right = vec3(
		global_data.data.view[0][0],
		global_data.data.view[1][0],
		global_data.data.view[2][0]
	);
	vec3 camera_up = vec3(
		global_data.data.view[0][1],
		global_data.data.view[1][1],
		global_data.data.view[2][1]
	);
	
	// Apply rotation to the local position
	float cos_rot = cos(in_rotation);
	float sin_rot = sin(in_rotation);
	mat2 rot_mat = mat2(
		cos_rot, -sin_rot,
		sin_rot, cos_rot
	);
	vec2 rotated_local = rot_mat * in_local_pos;
	
	// Calculate world position using billboard technique
	// The quad is constructed from camera_right and camera_up vectors
	vec3 world_pos = in_world_pos 
		+ camera_right * rotated_local.x * in_size
		+ camera_up * rotated_local.y * in_size;
	
	// Transform to clip space
	gl_Position = global_data.data.proj * global_data.data.view * vec4(world_pos, 1.0);
	
	// Pass through to fragment shader
	frag_tex_coord = in_tex_coord;
	frag_color = in_color;
}
