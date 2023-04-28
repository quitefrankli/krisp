#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtx/hash.hpp>


#define CPPONLY
#define VEC2 glm::vec2
#define VEC3 glm::vec3
#define MAT3 glm::mat3
#define MAT4 glm::mat4
#define CPP_MAT4_GLSL_MAT3 glm::mat4
#define ALIGN(X) alignas(X)

namespace SDS // stands for shared data structures
{
	#include "shared_data_structures.txt"
}

#undef VEC2
#undef VEC3
#undef MAT3
#undef MAT4
#undef CPP_MAT4_GLSL_MAT3
#undef ALIGN
#undef CPPONLY

namespace std
{
	template<>
	struct hash<SDS::ColorVertex>
	{
		size_t operator()(SDS::ColorVertex const& vertex) const
		{
			return hash<glm::vec3>()(vertex.pos) ^ hash<glm::vec3>()(vertex.normal);
		}
	};

	template<>
	struct hash<SDS::TexVertex>
	{
		size_t operator()(SDS::TexVertex const& vertex) const
		{
			return 
				hash<glm::vec3>()(vertex.pos) ^ hash<glm::vec3>()(vertex.normal) ^ 
				hash<glm::vec2>()(vertex.texCoord);
		}
	};
}