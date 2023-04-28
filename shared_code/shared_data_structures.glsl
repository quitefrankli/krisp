#define GLSLONLY
#define ALIGN(X) /* nothing */
#define VEC2 vec2
#define VEC3 vec3
#define MAT3 mat3
#define MAT4 mat4
#define CPP_MAT4_GLSL_MAT3 mat3

#include "shared_data_structures.txt"

#undef ALIGN
#undef VEC2
#undef VEC3
#undef MAT3
#undef MAT4
#undef CPP_MAT4_GLSL_MAT3
#undef GLSLONLY