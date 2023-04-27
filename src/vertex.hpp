// #pragma once

// #include <glm/vec2.hpp>
// #include <glm/vec3.hpp>

// #include <glm/gtx/hash.hpp>

// #include <vector>


// struct SDS::Vertex
// {
// 	glm::vec3 pos{};
// 	glm::vec3 color{};
// 	glm::vec2 texCoord{};
// 	glm::vec3 normal{};

// 	SDS::Vertex();
// 	SDS::Vertex(const SDS::Vertex&);
// 	SDS::Vertex(SDS::Vertex&& vertex) noexcept;
// 	SDS::Vertex(const glm::vec3& pos_, const glm::vec3& color_);
// 	SDS::Vertex(const glm::vec3& pos_, const glm::vec3& color_, const glm::vec2& texCoord_, const glm::vec3& normal_);

// 	SDS::Vertex& operator=(const SDS::Vertex& vertex);

// 	bool operator==(const SDS::Vertex& other) const
// 	{
// 		return pos == other.pos && color == other.color && texCoord == other.texCoord && normal == other.normal;
// 	}
// };

// namespace std
// {
// 	template<>
// 	struct hash<SDS::Vertex>
// 	{
// 		size_t operator()(SDS::Vertex const& vertex) const
// 		{
// 			return 
// 				hash<glm::vec3>()(vertex.pos) ^ 
// 				hash<glm::vec3>()(vertex.color) ^
// 				hash<glm::vec2>()(vertex.texCoord) ^
// 				hash<glm::vec3>()(vertex.normal);
// 		}
// 	};
// }