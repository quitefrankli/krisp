#pragma once

#include <glm/mat4x4.hpp>


// be vary of alignment issues
// a scalar of size N has a base alignment of N
// a 3/4 component vector/matrix with components of size N has a base alignment of 4N
struct UniformBufferObject
{
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
	glm::mat4 rot_mat; // we are really only sending a mat3 but due to alignment requiring vec4 we have to send a mat4
};

struct GlobalUniformBufferObject
{
	glm::vec3 light_pos;
	float lighting;
};