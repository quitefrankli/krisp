#include "shape.hpp"

#include "maths.hpp"

#include <unordered_map>
#include <algorithm>


ShapeID Shape::global_id = ShapeID(0);

// // this doesn't actually seem to work :/
// void Shape::deduplicate_vertices()
// {
// 	std::unordered_map<SDS::Vertex, uint32_t> unique_vertices;
// 	indices.clear();
// 	indices.reserve(vertices.size());

// 	uint32_t true_vertex_index = 0;
// 	for (auto& vertex : vertices)
// 	{
// 		auto unique_vertex_element = unique_vertices.try_emplace(std::move(vertex), true_vertex_index);
// 		if (unique_vertex_element.second)
// 		{
// 			// new vertex
// 			vertices[true_vertex_index] = vertex;
// 			indices.emplace_back(true_vertex_index++);
// 		} else
// 		{
// 			// if already occupied, set index to the index in which said vertex occupies
// 			indices.emplace_back(unique_vertex_element.first->second);
// 		}
// 	}
// }

template<typename VertexType>
static void Shape::transform_vertices(std::vector<VertexType>& vertices, const glm::mat4& transform)
{
	for (auto& vertex : vertices)
	{
		vertex.pos = glm::vec3(transform * glm::vec4(vertex.pos, 1));
		vertex.normal = glm::normalize(glm::vec3(transform * glm::vec4(vertex.normal, 1)));
	}
}

template<typename VertexType>
static void Shape::transform_vertices(std::vector<VertexType>& vertices, const glm::quat& quat)
{
	for (auto& vertex : vertices)
	{
		vertex.pos = quat * vertex.pos;
		vertex.normal = glm::normalize(quat * vertex.normal);
	}
}

template<typename VertexType>
static void Shape::translate_vertices(std::vector<VertexType>& vertices, const glm::vec3& vec)
{
	for (auto& vertex : vertices)
	{
		vertex.pos += vec;
	}
}

template<typename VertexType>
static void Shape::generate_normals(std::vector<VertexType>& vertices, std::vector<uint32_t>& indices)
{
	// zero all normals
	std::for_each(vertices.begin(), vertices.end(), [](VertexType& vertex){vertex.normal = glm::vec3(0.0f);});
	
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

template<typename VertexType>
AABB Shape::calculate_bounding_box(const std::vector<VertexType>& vertices)
{
	AABB aabb(glm::vec3(std::numeric_limits<float>::max()), glm::vec3(-std::numeric_limits<float>::max()));
	for (const VertexType& vertex : vertices)
	{
		auto& p = vertex.pos;
		aabb.min_bound.x = std::min<float>(aabb.min_bound.x, p.x);
		aabb.min_bound.y = std::min<float>(aabb.min_bound.y, p.y);
		aabb.min_bound.z = std::min<float>(aabb.min_bound.z, p.z);

		aabb.max_bound.x = std::max<float>(aabb.max_bound.x, p.x);
		aabb.max_bound.y = std::max<float>(aabb.max_bound.y, p.y);
		aabb.max_bound.z = std::max<float>(aabb.max_bound.z, p.z);
	}

	return aabb;
}

//
// template method instantiations
//

template void Shape::transform_vertices(std::vector<SDS::ColorVertex>& vertices, const glm::mat4& transform);
template void Shape::transform_vertices(std::vector<SDS::TexVertex>& vertices, const glm::mat4& transform);
template void Shape::transform_vertices(std::vector<SDS::ColorVertex>& vertices, const glm::quat& quat);
template void Shape::transform_vertices(std::vector<SDS::TexVertex>& vertices, const glm::quat& quat);
template void Shape::translate_vertices(std::vector<SDS::ColorVertex>& vertices, const glm::vec3& vec);
template void Shape::translate_vertices(std::vector<SDS::TexVertex>& vertices, const glm::vec3& vec);
template void Shape::generate_normals(std::vector<SDS::ColorVertex>& vertices, std::vector<uint32_t>& indices);
template void Shape::generate_normals(std::vector<SDS::TexVertex>& vertices, std::vector<uint32_t>& indices);
template AABB Shape::calculate_bounding_box(const std::vector<SDS::ColorVertex>& vertices);
template AABB Shape::calculate_bounding_box(const std::vector<SDS::TexVertex>& vertices);