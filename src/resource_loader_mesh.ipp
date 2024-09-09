#include "resource_loader.hpp"
#include "renderable/mesh.hpp"

#include <tiny_gltf.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <fmt/core.h>

#include <iostream>
#include <unordered_set>


struct GLTFVertexData
{
	const int data_component_type;
	const int data_type;
	const size_t count;
	const void* data;
};

const GLTFVertexData _get_generic_vertex_data(
	const std::string_view attribute_name,
	std::unordered_set<int> supported_data_component_types,
	std::unordered_set<int> supported_data_types,
	const tinygltf::Model& model,
	const tinygltf::Primitive& primitive)
{
	const auto it_attr = primitive.attributes.find(attribute_name.data());
	if (it_attr == primitive.attributes.end())
	{
		throw std::runtime_error(fmt::format("ResourceLoader: Primitive does not have {} attribute", attribute_name));
	}

	const auto& accessor = model.accessors[it_attr->second];
	
	if (!supported_data_component_types.contains(accessor.componentType) || 
		!supported_data_types.contains(accessor.type))
	{
		throw std::runtime_error(fmt::format("ResourceLoader: componentType or type is not supported for {}", attribute_name));
	}

	const auto& buffer_view = model.bufferViews[accessor.bufferView];
	const auto& buffer = model.buffers[buffer_view.buffer];

	return GLTFVertexData{ 
		accessor.componentType, 
		accessor.type,
		accessor.count,
		buffer.data.data() + buffer_view.byteOffset + accessor.byteOffset
	};
}

const GLTFVertexData get_pos_data(const tinygltf::Model& model, const tinygltf::Primitive& primitive)
{
	return _get_generic_vertex_data(
		"POSITION", 
		{ TINYGLTF_COMPONENT_TYPE_FLOAT }, 
		{ TINYGLTF_TYPE_VEC3 }, 
		model, 
		primitive);
}

const GLTFVertexData get_normal_data(const tinygltf::Model& model, const tinygltf::Primitive& primitive)
{
	return _get_generic_vertex_data(
		"NORMAL", 
		{ TINYGLTF_COMPONENT_TYPE_FLOAT }, 
		{ TINYGLTF_TYPE_VEC3 }, 
		model, 
		primitive);
}

const GLTFVertexData get_tex_data(const tinygltf::Model& model, const tinygltf::Primitive& primitive)
{
	return _get_generic_vertex_data(
		"TEXCOORD_0", 
		{ TINYGLTF_COMPONENT_TYPE_FLOAT }, 
		{ TINYGLTF_TYPE_VEC2 }, 
		model, 
		primitive);
}

const GLTFVertexData get_joint_data(const tinygltf::Model& model, const tinygltf::Primitive& primitive)
{
	return _get_generic_vertex_data(
		"JOINTS_0", 
		{ TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE,TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT }, 
		{ TINYGLTF_TYPE_VEC4 }, 
		model, 
		primitive);
}

const GLTFVertexData get_weight_data(const tinygltf::Model& model, const tinygltf::Primitive& primitive)
{
	return _get_generic_vertex_data(
		"WEIGHTS_0", 
		{ TINYGLTF_COMPONENT_TYPE_FLOAT }, 
		{ TINYGLTF_TYPE_VEC4 }, 
		model, 
		primitive);
}

template<typename src_t, typename dst_t>
void copy_data(const src_t* src, dst_t& dst, const uint32_t idx)
{
	const size_t start_offset = dst_t::length() * idx;
	const size_t end_offset = start_offset + dst_t::length();
	std::copy(src+start_offset, src+end_offset, glm::value_ptr(dst));
}

template<typename PosType, typename NormType>
ColorVertices load_color_vertices(
	const size_t count, 
	const PosType* pos_data, 
	const NormType* norm_data);

template<>
ColorVertices load_color_vertices(
	const size_t count, 
	const float* pos_data, 
	const float* norm_data)
{
	ColorVertices vertices(count);
	for (size_t i = 0; i < count; ++i)
	{
		auto& vertex = vertices[i];
		copy_data(pos_data, vertex.pos, i);
		copy_data(norm_data, vertex.normal, i);
	}

	return vertices;
}

template<typename PosType, typename NormType, typename TexType>
TexVertices load_tex_vertices(
	const size_t count, 
	const PosType* pos_data, 
	const NormType* norm_data, 
	const TexType* tex_data);

template<>
TexVertices load_tex_vertices(
	const size_t count, 
	const float* pos_data, 
	const float* norm_data, 
	const float* tex_data)
{
	TexVertices vertices(count);
	for (size_t i = 0; i < count; ++i)
	{
		auto& vertex = vertices[i];
		copy_data(pos_data, vertex.pos, i);
		copy_data(norm_data, vertex.normal, i);
		copy_data(tex_data, vertex.texCoord, i);
	}

	return vertices;
}

template<typename PosType, 
		 typename NormType, 
		 typename TexType, 
		 typename JointType, 
		 typename WeightType>
SkinnedVertices load_skinned_vertices(
	const size_t count, 
	const PosType* pos_data, 
	const NormType* norm_data, 
	const TexType* tex_data, 
	const JointType* joint_data, 
	const WeightType* weight_data);

template<>
SkinnedVertices load_skinned_vertices(
	const size_t count, 
	const float* pos_data, 
	const float* norm_data, 
	const float* tex_data, 
	const uint8_t* joint_data, 
	const float* weight_data)
{
	SkinnedVertices vertices(count);
	for (size_t i = 0; i < count; ++i)
	{
		auto& vertex = vertices[i];
		copy_data(pos_data, vertex.pos, i);
		copy_data(norm_data, vertex.normal, i);
		copy_data(tex_data, vertex.texCoord, i);
		copy_data(joint_data, vertex.bone_ids, i);
		copy_data(weight_data, vertex.bone_weights, i);
	}

	return vertices;
}

template<>
SkinnedVertices load_skinned_vertices(
	const size_t count, 
	const float* pos_data, 
	const float* norm_data, 
	const float* tex_data, 
	const uint16_t* joint_data, 
	const float* weight_data)
{
	SkinnedVertices vertices(count);
	for (size_t i = 0; i < count; ++i)
	{
		auto& vertex = vertices[i];
		copy_data(pos_data, vertex.pos, i);
		copy_data(norm_data, vertex.normal, i);
		copy_data(tex_data, vertex.texCoord, i);
		copy_data(joint_data, vertex.bone_ids, i);
		copy_data(weight_data, vertex.bone_weights, i);
	}

	return vertices;
}

template<typename VerticesType>
VerticesType load_vertices(
	const tinygltf::Model& model, 
	tinygltf::Primitive& primitive);

template<>
ColorVertices load_vertices<ColorVertices>(const tinygltf::Model& model, tinygltf::Primitive& primitive)
{
	const auto pos_data = get_pos_data(model, primitive);
	const auto normal_data = get_normal_data(model, primitive);

	return load_color_vertices(
		pos_data.count,
		reinterpret_cast<const float*>(pos_data.data),
		reinterpret_cast<const float*>(normal_data.data));
}

template<>
TexVertices load_vertices<TexVertices>(const tinygltf::Model& model, tinygltf::Primitive& primitive)
{
	const auto pos_data = get_pos_data(model, primitive);
	const auto normal_data = get_normal_data(model, primitive);
	const auto tex_data = get_tex_data(model, primitive);

	return load_tex_vertices(
		pos_data.count,
		reinterpret_cast<const float*>(pos_data.data),
		reinterpret_cast<const float*>(normal_data.data),
		reinterpret_cast<const float*>(tex_data.data));
}

template<>
SkinnedVertices load_vertices<SkinnedVertices>(const tinygltf::Model& model, tinygltf::Primitive& primitive)
{
	const auto pos_data = get_pos_data(model, primitive);
	const auto normal_data = get_normal_data(model, primitive);
	const auto tex_data = get_tex_data(model, primitive);
	const auto joint_data = get_joint_data(model, primitive);
	const auto weight_data = get_weight_data(model, primitive);

	return load_skinned_vertices(
		pos_data.count,
		reinterpret_cast<const float*>(pos_data.data),
		reinterpret_cast<const float*>(normal_data.data),
		reinterpret_cast<const float*>(tex_data.data),
		reinterpret_cast<const uint8_t*>(joint_data.data),
		reinterpret_cast<const float*>(weight_data.data));
}