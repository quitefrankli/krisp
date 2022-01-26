#include "vertex.hpp"


Vertex::Vertex() = default;
Vertex::Vertex(const Vertex & vertex) = default;
Vertex::Vertex(Vertex&& vertex) noexcept = default;
Vertex::Vertex(const glm::vec3& pos_, const glm::vec3& color_) :
	pos(pos_), color(color_)
{
}
Vertex::Vertex(const glm::vec3& pos_, const glm::vec3& color_, const glm::vec2& texCoord_, const glm::vec3& normal_) :
	pos(pos_), color(color_), texCoord(texCoord_), normal(normal_)
{
}
Vertex& Vertex::operator=(const Vertex& vertex) = default;