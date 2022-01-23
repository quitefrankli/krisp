#include "shape.hpp"


void Shape::transform_vertices(const glm::mat4& transform)
{
	for (auto& vertex : vertices)
	{
		vertex.pos = glm::vec3(transform * glm::vec4(vertex.pos, 1));
		vertex.normal = glm::normalize(glm::vec3(transform * glm::vec4(vertex.normal, 1)));
	}
}

void Shape::generate_normals()
{
	// zero all normals
	std::for_each(vertices.begin(), vertices.end(), [](Vertex& vertex){vertex.normal = glm::vec3(0.0f);});
	
	// used for counting, after which we take the average of the normals
	std::vector<uint32_t> counter(indices.size(), 0);


	// every offset of 3 is a new triangle, we generate the normal by taking
	// the cross product of the sides
	for (int i = 0; i < indices.size(); i+=3)
	{
		glm::vec3 v1 = vertices[indices[i+1]].pos - vertices[indices[i]].pos;
		glm::vec3 v2 = vertices[indices[i+2]].pos - vertices[indices[i]].pos;
		glm::vec3 normal = glm::normalize(glm::cross(v1, v2));
		vertices[indices[i]].normal += normal;
		vertices[indices[i+1]].normal += normal;
		vertices[indices[i+2]].normal += normal;
		counter[indices[i]]++;
		counter[indices[i+1]]++;
		counter[indices[i+2]]++;
	}

	// 2nd pass we take the average
	for (const uint32_t index : indices)
	{
		vertices[index].normal = glm::normalize(vertices[index].normal / (float)counter[index]);
	}
}