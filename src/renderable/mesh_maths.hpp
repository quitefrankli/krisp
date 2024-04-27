#pragma once

#include "collision/bounding_box.hpp"


template<typename VertexType>
void transform_vertices(std::vector<VertexType>& vertices, const glm::mat4& transform);

template<typename VertexType>
void transform_vertices(std::vector<VertexType>& vertices, const glm::quat& quat);

template<typename VertexType>
void translate_vertices(std::vector<VertexType>& vertices, const glm::vec3& vec);

template<typename VertexType>
void scale_vertices(std::vector<VertexType>& vertices, const glm::vec3& vec);

template<typename VertexType>
void concatenate_vertices(std::vector<VertexType>& vertices, 
						  std::vector<uint32_t>& indices,
						  const std::vector<VertexType>& other_vertices,
						  const std::vector<uint32_t>& other_indices);

template<typename VertexType>
void generate_normals(std::vector<VertexType>& vertices, std::vector<uint32_t>& indices);

template<typename VertexType>
AABB calculate_bounding_box(const std::vector<VertexType>& vertices);

template<typename PrimitiveType, typename VertexType>
void calculate_bounding_primitive(const std::vector<VertexType>& vertices);