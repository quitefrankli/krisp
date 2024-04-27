#include "mesh_maths.hpp"
#include "shared_data_structures.hpp"
#include "collision/bounding_box.hpp"

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <vector>


template<typename VertexType>
void transform_vertices(std::vector<VertexType>& vertices, const glm::mat4& transform)
{
	for (auto& vertex : vertices)
	{
		vertex.pos = glm::vec3(transform * glm::vec4(vertex.pos, 1));
		vertex.normal = glm::normalize(glm::vec3(transform * glm::vec4(vertex.normal, 1)));
	}
}

template<typename VertexType>
void transform_vertices(std::vector<VertexType>& vertices, const glm::quat& quat)
{
	for (auto& vertex : vertices)
	{
		vertex.pos = quat * vertex.pos;
		vertex.normal = glm::normalize(quat * vertex.normal);
	}
}

template<typename VertexType>
void translate_vertices(std::vector<VertexType>& vertices, const glm::vec3& vec)
{
	for (auto& vertex : vertices)
	{
		vertex.pos += vec;
	}
}

template<typename VertexType>
void scale_vertices(std::vector<VertexType>& vertices, const glm::vec3& vec)
{
	for (auto& vertex : vertices)
	{
		vertex.pos *= vec;
	}
}

template<typename VertexType>
void concatenate_vertices(std::vector<VertexType>& vertices,
                          std::vector<uint32_t>& indices,
                          const std::vector<VertexType>& other_vertices,
                          const std::vector<uint32_t>& other_indices)
{
	indices.insert(indices.end(), other_indices.begin(), other_indices.end());
	for (int i = indices.size() - other_indices.size(); i < indices.size(); i++)
	{
		indices[i] += vertices.size();
	}
	vertices.insert(vertices.end(), other_vertices.begin(), other_vertices.end());
}

template<typename VertexType>
void generate_normals(std::vector<VertexType>& vertices, std::vector<uint32_t>& indices)
{
	// zero all normals
	std::for_each(vertices.begin(), vertices.end(), [](VertexType& vertex){vertex.normal = Maths::zero_vec;});
	
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
AABB calculate_bounding_box(const std::vector<VertexType>& vertices)
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

template<typename PrimitiveType, typename VertexType>
void calculate_bounding_primitive(const std::vector<VertexType>& vertices)
{
	// TODO: implement me properly
	assert(false);
	
	// AABB aabb(
	// { 
	// 	std::numeric_limits<float>::max(),
	// 	std::numeric_limits<float>::max(),
	// 	std::numeric_limits<float>::max()
	// },
	// {
	// 	-std::numeric_limits<float>::max(),
	// 	-std::numeric_limits<float>::max(),
	// 	-std::numeric_limits<float>::max()
	// });

	// for (const auto& renderable : renderables)
	// {
		
	// 	aabb.min_max(shape->calculate_bounding_box());
	// }

	// bounding_primitive_sphere.origin = (aabb.min_bound + aabb.max_bound) / 2.0f;
	// bounding_primitive_sphere.radius = glm::compMax(glm::abs(aabb.max_bound - aabb.min_bound))/2.0f;

	// is_bounding_primitive_cached = true;
}


// instantiate the template methods
template void transform_vertices<SDS::ColorVertex>(std::vector<SDS::ColorVertex>& vertices, const glm::mat4& transform);
template void transform_vertices<SDS::ColorVertex>(std::vector<SDS::ColorVertex>& vertices, const glm::quat& quat);
template void translate_vertices<SDS::ColorVertex>(std::vector<SDS::ColorVertex>& vertices, const glm::vec3& vec);
template void generate_normals<SDS::ColorVertex>(std::vector<SDS::ColorVertex>& vertices, std::vector<uint32_t>& indices);
template void concatenate_vertices<SDS::ColorVertex>(std::vector<SDS::ColorVertex>& vertices, 
													 std::vector<uint32_t>& indices,
													 const std::vector<SDS::ColorVertex>& other_vertices,
													 const std::vector<uint32_t>& other_indices);

template void transform_vertices<SDS::TexVertex>(std::vector<SDS::TexVertex>& vertices, const glm::mat4& transform);
template void transform_vertices<SDS::TexVertex>(std::vector<SDS::TexVertex>& vertices, const glm::quat& quat);
template void translate_vertices<SDS::TexVertex>(std::vector<SDS::TexVertex>& vertices, const glm::vec3& vec);
template void generate_normals<SDS::TexVertex>(std::vector<SDS::TexVertex>& vertices, std::vector<uint32_t>& indices);
template void concatenate_vertices<SDS::TexVertex>(std::vector<SDS::TexVertex>& vertices, 
												   std::vector<uint32_t>& indices,
												   const std::vector<SDS::TexVertex>& other_vertices,
												   const std::vector<uint32_t>& other_indices);

template void transform_vertices<SDS::SkinnedVertex>(std::vector<SDS::SkinnedVertex>& vertices, const glm::mat4& transform);
template void transform_vertices<SDS::SkinnedVertex>(std::vector<SDS::SkinnedVertex>& vertices, const glm::quat& quat);
template void translate_vertices<SDS::SkinnedVertex>(std::vector<SDS::SkinnedVertex>& vertices, const glm::vec3& vec);
template void generate_normals<SDS::SkinnedVertex>(std::vector<SDS::SkinnedVertex>& vertices, std::vector<uint32_t>& indices);
template void concatenate_vertices<SDS::SkinnedVertex>(std::vector<SDS::SkinnedVertex>& vertices, 
													   std::vector<uint32_t>& indices,
													   const std::vector<SDS::SkinnedVertex>& other_vertices,
													   const std::vector<uint32_t>& other_indices);

