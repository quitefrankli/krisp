#include "resource_loader.hpp"
#include "renderable/mesh.hpp"

#include <tiny_gltf.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>

namespace GltfImport
{
inline size_t component_size(const int component_type)
{
	switch (component_type)
	{
		case TINYGLTF_COMPONENT_TYPE_BYTE:
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: return 1;
		case TINYGLTF_COMPONENT_TYPE_SHORT:
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: return 2;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
		case TINYGLTF_COMPONENT_TYPE_FLOAT: return 4;
		default: throw std::runtime_error("ResourceLoader: unsupported accessor component type");
	}
}

inline size_t component_count(const int type)
{
	switch (type)
	{
		case TINYGLTF_TYPE_SCALAR: return 1;
		case TINYGLTF_TYPE_VEC2: return 2;
		case TINYGLTF_TYPE_VEC3: return 3;
		case TINYGLTF_TYPE_VEC4: return 4;
		case TINYGLTF_TYPE_MAT4: return 16;
		default: throw std::runtime_error("ResourceLoader: unsupported accessor type");
	}
}

struct AccessorReader
{
	const tinygltf::Accessor& accessor;
	const tinygltf::BufferView& view;
	const tinygltf::Buffer& buffer;

	AccessorReader(const tinygltf::Model& model, const int accessor_index) :
		accessor(model.accessors.at(accessor_index)),
		view(get_view(model, accessor)),
		buffer(model.buffers.at(view.buffer))
	{
	}

	static const tinygltf::BufferView& get_view(const tinygltf::Model& model, const tinygltf::Accessor& accessor)
	{
		if (accessor.bufferView < 0)
			throw std::runtime_error("ResourceLoader: sparse accessors are not supported");
		return model.bufferViews.at(accessor.bufferView);
	}

	size_t stride() const
	{
		const size_t packed = component_size(accessor.componentType) * component_count(accessor.type);
		return view.byteStride == 0 ? packed : view.byteStride;
	}

	const unsigned char* element(const size_t index) const
	{
		if (index >= accessor.count)
		{
			throw std::out_of_range("ResourceLoader: accessor index is out of range");
		}
		return buffer.data.data() + view.byteOffset + accessor.byteOffset + index * stride();
	}

	double number(const size_t index, const size_t component, const bool normalize = true) const
	{
		const auto* ptr = element(index) + component * component_size(accessor.componentType);
		switch (accessor.componentType)
		{
			case TINYGLTF_COMPONENT_TYPE_BYTE:
			{
				int8_t value; std::memcpy(&value, ptr, sizeof(value));
				return accessor.normalized && normalize ? std::max(-1.0, value / 127.0) : value;
			}
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
			{
				uint8_t value; std::memcpy(&value, ptr, sizeof(value));
				return accessor.normalized && normalize ? value / 255.0 : value;
			}
			case TINYGLTF_COMPONENT_TYPE_SHORT:
			{
				int16_t value; std::memcpy(&value, ptr, sizeof(value));
				return accessor.normalized && normalize ? std::max(-1.0, value / 32767.0) : value;
			}
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
			{
				uint16_t value; std::memcpy(&value, ptr, sizeof(value));
				return accessor.normalized && normalize ? value / 65535.0 : value;
			}
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
			{
				uint32_t value; std::memcpy(&value, ptr, sizeof(value));
				return accessor.normalized && normalize ? value / 4294967295.0 : value;
			}
			case TINYGLTF_COMPONENT_TYPE_FLOAT:
			{
				float value; std::memcpy(&value, ptr, sizeof(value));
				return value;
			}
			default: throw std::runtime_error("ResourceLoader: unsupported accessor component type");
		}
	}
};

inline const tinygltf::Accessor& find_accessor(
	const tinygltf::Model& model,
	const tinygltf::Primitive& primitive,
	const char* semantic)
{
	const auto it = primitive.attributes.find(semantic);
	if (it == primitive.attributes.end())
	{
		throw std::runtime_error(std::string("ResourceLoader: primitive is missing ") + semantic);
	}
	return model.accessors.at(it->second);
}

inline bool has_attribute(const tinygltf::Primitive& primitive, const char* semantic)
{
	return primitive.attributes.contains(semantic);
}

inline std::vector<glm::vec2> read_vec2(const tinygltf::Model& model, const int index)
{
	AccessorReader reader(model, index);
	if (reader.accessor.type != TINYGLTF_TYPE_VEC2)
		throw std::runtime_error("ResourceLoader: expected a vec2 accessor");
	std::vector<glm::vec2> values(reader.accessor.count);
	for (size_t i = 0; i < values.size(); ++i)
		values[i] = { reader.number(i, 0), reader.number(i, 1) };
	return values;
}

inline std::vector<glm::vec3> read_vec3(const tinygltf::Model& model, const int index)
{
	AccessorReader reader(model, index);
	if (reader.accessor.type != TINYGLTF_TYPE_VEC3)
		throw std::runtime_error("ResourceLoader: expected a vec3 accessor");
	std::vector<glm::vec3> values(reader.accessor.count);
	for (size_t i = 0; i < values.size(); ++i)
		values[i] = { reader.number(i, 0), reader.number(i, 1), reader.number(i, 2) };
	return values;
}

inline std::vector<glm::vec4> read_vec4(const tinygltf::Model& model, const int index, const bool normalize = true)
{
	AccessorReader reader(model, index);
	if (reader.accessor.type != TINYGLTF_TYPE_VEC4)
		throw std::runtime_error("ResourceLoader: expected a vec4 accessor");
	std::vector<glm::vec4> values(reader.accessor.count);
	for (size_t i = 0; i < values.size(); ++i)
		values[i] = { reader.number(i, 0, normalize), reader.number(i, 1, normalize),
			reader.number(i, 2, normalize), reader.number(i, 3, normalize) };
	return values;
}

inline std::vector<uint32_t> read_indices(const tinygltf::Model& model, const tinygltf::Primitive& primitive, const size_t vertex_count)
{
	if (primitive.indices < 0)
	{
		std::vector<uint32_t> indices(vertex_count);
		for (uint32_t i = 0; i < indices.size(); ++i) indices[i] = i;
		return indices;
	}

	AccessorReader reader(model, primitive.indices);
	if (reader.accessor.type != TINYGLTF_TYPE_SCALAR)
		throw std::runtime_error("ResourceLoader: index accessor must be scalar");
	std::vector<uint32_t> indices(reader.accessor.count);
	for (size_t i = 0; i < indices.size(); ++i)
		indices[i] = static_cast<uint32_t>(reader.number(i, 0, false));
	return indices;
}

inline std::vector<uint32_t> triangles_from(
	const tinygltf::Primitive& primitive,
	std::vector<uint32_t> indices,
	const bool allow_non_triangles)
{
	if (primitive.mode == TINYGLTF_MODE_TRIANGLES)
		return indices;
	if (!allow_non_triangles)
		throw std::runtime_error("ResourceLoader: only triangle primitives are enabled");

	std::vector<uint32_t> triangles;
	if (primitive.mode == TINYGLTF_MODE_TRIANGLE_STRIP)
	{
		for (size_t i = 2; i < indices.size(); ++i)
		{
			if (i % 2 == 0) triangles.insert(triangles.end(), { indices[i - 2], indices[i - 1], indices[i] });
			else triangles.insert(triangles.end(), { indices[i - 1], indices[i - 2], indices[i] });
		}
		return triangles;
	}
	if (primitive.mode == TINYGLTF_MODE_TRIANGLE_FAN)
	{
		for (size_t i = 2; i < indices.size(); ++i)
			triangles.insert(triangles.end(), { indices[0], indices[i - 1], indices[i] });
		return triangles;
	}
	throw std::runtime_error("ResourceLoader: primitive mode is not supported");
}

inline std::vector<glm::vec3> generate_normals(
	const std::vector<glm::vec3>& positions,
	const std::vector<uint32_t>& indices)
{
	std::vector<glm::vec3> normals(positions.size(), glm::vec3(0.0f));
	for (size_t i = 0; i + 2 < indices.size(); i += 3)
	{
		const glm::vec3 cross = glm::cross(
			positions.at(indices[i + 1]) - positions.at(indices[i]),
			positions.at(indices[i + 2]) - positions.at(indices[i]));
		normals.at(indices[i]) += cross;
		normals.at(indices[i + 1]) += cross;
		normals.at(indices[i + 2]) += cross;
	}
	for (auto& normal : normals)
		normal = glm::length(normal) > 0.0f ? glm::normalize(normal) : Maths::up_vec;
	return normals;
}
}

inline ColorVertices load_color_vertices(
	const std::vector<glm::vec3>& positions,
	const std::vector<glm::vec3>& normals)
{
	ColorVertices vertices(positions.size());
	for (size_t i = 0; i < vertices.size(); ++i)
	{
		vertices[i].pos = positions[i];
		vertices[i].normal = normals[i];
	}
	return vertices;
}

inline TexVertices load_tex_vertices(
	const std::vector<glm::vec3>& positions,
	const std::vector<glm::vec3>& normals,
	const std::vector<glm::vec2>& texcoords)
{
	TexVertices vertices(positions.size());
	for (size_t i = 0; i < vertices.size(); ++i)
	{
		vertices[i].pos = positions[i];
		vertices[i].normal = normals[i];
		vertices[i].texCoord = texcoords[i];
	}
	return vertices;
}

inline SkinnedVertices load_skinned_vertices(
	const std::vector<glm::vec3>& positions,
	const std::vector<glm::vec3>& normals,
	const std::vector<glm::vec2>& texcoords,
	const std::vector<glm::vec4>& joints,
	const std::vector<glm::vec4>& weights)
{
	SkinnedVertices vertices(positions.size());
	for (size_t i = 0; i < vertices.size(); ++i)
	{
		vertices[i].pos = positions[i];
		vertices[i].normal = normals[i];
		vertices[i].texCoord = texcoords[i];
		vertices[i].bone_ids = joints[i];
		vertices[i].bone_weights = weights[i];
	}
	return vertices;
}
