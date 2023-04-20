#pragma once

#define VEC3 glm::vec3
#define MAT3 glm::mat3
#define MAT4 glm::mat4
#define CPP_MAT4_GLSL_MAT3 glm::mat4
#define ALIGN(X) alignas(X)

namespace SDS // stands for shared data structures
{
	#include "shared_data_structures.txt"
}

#undef VEC3
#undef MAT3
#undef MAT4
#undef CPP_MAT4_GLSL_MAT3
#undef ALIGN