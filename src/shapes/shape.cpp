#include "shape.hpp"

#include "maths.hpp"


void Shape::transform_vertices(const glm::mat4& transform)
{
	for (auto& vertex : vertices)
	{
		vertex.pos = glm::vec3(transform * glm::vec4(vertex.pos, 1));
		vertex.normal = glm::normalize(glm::vec3(transform * glm::vec4(vertex.normal, 1)));
	}
}

void Shape::transform_vertices(const glm::quat& quat)
{
	for (auto& vertex : vertices)
	{
		vertex.pos = quat * vertex.pos;
		vertex.normal = glm::normalize(quat * vertex.normal);
	}
}

void Shape::translate_vertices(const glm::vec3& vec)
{
	for (auto& vertex : vertices)
	{
		vertex.pos += vec;
		// no rotation so no change to the normals
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

// this doesn't actually seem to work :/
void Shape::deduplicate_vertices()
{
	std::unordered_map<Vertex, uint32_t> unique_vertices;
	indices.clear();
	indices.reserve(vertices.size());

	uint32_t true_vertex_index = 0;
	for (auto& vertex : vertices)
	{
		auto& unique_vertex_element = unique_vertices.try_emplace(std::move(vertex), true_vertex_index);
		if (unique_vertex_element.second)
		{
			// new vertex
			vertices[true_vertex_index] = vertex;
			indices.emplace_back(true_vertex_index++);
		} else
		{
			// if already occupied, set index to the index in which said vertex occupies
			indices.emplace_back(unique_vertex_element.first->second);
		}
	}
}

bool Shape::check_collision(const Maths::Ray& ray) { return true; }