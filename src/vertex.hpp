#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <glm/gtx/hash.hpp>

#include <vector>


struct Vertex
{
	glm::vec3 pos{};
	glm::vec3 color{};
	glm::vec2 texCoord{};
	glm::vec3 normal{};

	Vertex();
	Vertex(const Vertex&);
	Vertex(Vertex&& vertex) noexcept;
	Vertex(const glm::vec3& pos_, const glm::vec3& color_);
	Vertex(const glm::vec3& pos_, const glm::vec3& color_, const glm::vec2& texCoord_, const glm::vec3& normal_);

	Vertex& operator=(const Vertex& vertex);

	bool operator==(const Vertex& other) const
	{
		return pos == other.pos && color == other.color && texCoord == other.texCoord && normal == other.normal;
	}
};

namespace std
{
	template<>
	struct hash<Vertex>
	{
		size_t operator()(Vertex const& vertex) const
		{
			return 
				hash<glm::vec3>()(vertex.pos) ^ 
				hash<glm::vec3>()(vertex.color) ^
				hash<glm::vec2>()(vertex.texCoord) ^
				hash<glm::vec3>()(vertex.normal);
		}
	};
}