#include "resource_loader.hpp"
#include "renderable/mesh.hpp"

#include <tiny_gltf.h>
#include <MikkTSpace/mikktspace.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <cmath>
#include <stdexcept>
#include <unordered_map>
#include <limits>

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
		default: throw ResourceLoadError("ResourceLoader: unsupported accessor component type");
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
		default: throw ResourceLoadError("ResourceLoader: unsupported accessor type");
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
		const size_t packed = component_size(accessor.componentType) * component_count(accessor.type);
		const size_t element_stride = stride();
		if (element_stride < packed || element_stride % component_size(accessor.componentType) != 0)
			throw ResourceLoadError("ResourceLoader: accessor has an invalid byte stride");
		if (view.byteOffset > buffer.data.size() || view.byteLength > buffer.data.size() - view.byteOffset)
			throw ResourceLoadError("ResourceLoader: buffer view is outside its buffer");
		if (accessor.byteOffset > view.byteLength)
			throw ResourceLoadError("ResourceLoader: accessor offset is outside its buffer view");
		if (accessor.count > 0)
		{
			const size_t remaining = view.byteLength - accessor.byteOffset;
			if (accessor.count - 1 > (std::numeric_limits<size_t>::max() - packed) / element_stride)
				throw ResourceLoadError("ResourceLoader: accessor byte range overflows");
			const size_t required = (accessor.count - 1) * element_stride + packed;
			if (required > remaining)
				throw ResourceLoadError("ResourceLoader: accessor data is outside its buffer view");
		}
	}

	static const tinygltf::BufferView& get_view(const tinygltf::Model& model, const tinygltf::Accessor& accessor)
	{
		if (accessor.bufferView < 0)
			throw ResourceLoadError("ResourceLoader: sparse accessors are not supported");
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
			default: throw ResourceLoadError("ResourceLoader: unsupported accessor component type");
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
		throw ResourceLoadError(std::string("ResourceLoader: primitive is missing ") + semantic);
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
		throw ResourceLoadError("ResourceLoader: expected a vec2 accessor");
	std::vector<glm::vec2> values(reader.accessor.count);
	for (size_t i = 0; i < values.size(); ++i)
		values[i] = { reader.number(i, 0), reader.number(i, 1) };
	return values;
}

inline std::vector<glm::vec3> read_vec3(const tinygltf::Model& model, const int index)
{
	AccessorReader reader(model, index);
	if (reader.accessor.type != TINYGLTF_TYPE_VEC3)
		throw ResourceLoadError("ResourceLoader: expected a vec3 accessor");
	std::vector<glm::vec3> values(reader.accessor.count);
	for (size_t i = 0; i < values.size(); ++i)
		values[i] = { reader.number(i, 0), reader.number(i, 1), reader.number(i, 2) };
	return values;
}

inline std::vector<glm::vec4> read_vec4(const tinygltf::Model& model, const int index, const bool normalize = true)
{
	AccessorReader reader(model, index);
	if (reader.accessor.type != TINYGLTF_TYPE_VEC4)
		throw ResourceLoadError("ResourceLoader: expected a vec4 accessor");
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
		throw ResourceLoadError("ResourceLoader: index accessor must be scalar");
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
		throw ResourceLoadError("ResourceLoader: only triangle primitives are enabled");

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
	throw ResourceLoadError("ResourceLoader: primitive mode is not supported");
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

struct TangentGenerationInput
{
	const std::vector<glm::vec3>& positions;
	const std::vector<glm::vec3>& normals;
	const std::vector<glm::vec2>& texcoords;
	const std::vector<uint32_t>& indices;
	std::vector<glm::vec4> corner_tangents;
};

inline uint32_t tangent_source_index(const TangentGenerationInput& input, const int face, const int vertex)
{
	return input.indices.at(static_cast<size_t>(face) * 3 + vertex);
}

inline TangentGenerationInput& tangent_input(const SMikkTSpaceContext* context)
{
	return *static_cast<TangentGenerationInput*>(context->m_pUserData);
}

inline int tangent_face_count(const SMikkTSpaceContext* context)
{
	return static_cast<int>(tangent_input(context).indices.size() / 3);
}

inline int tangent_vertices_per_face(const SMikkTSpaceContext*, const int)
{
	return 3;
}

inline void tangent_position(const SMikkTSpaceContext* context, float out[], const int face, const int vertex)
{
	const auto& input = tangent_input(context);
	const auto& value = input.positions.at(tangent_source_index(input, face, vertex));
	out[0] = value.x;
	out[1] = value.y;
	out[2] = value.z;
}

inline void tangent_normal(const SMikkTSpaceContext* context, float out[], const int face, const int vertex)
{
	const auto& input = tangent_input(context);
	const auto& value = input.normals.at(tangent_source_index(input, face, vertex));
	out[0] = value.x;
	out[1] = value.y;
	out[2] = value.z;
}

inline void tangent_texcoord(const SMikkTSpaceContext* context, float out[], const int face, const int vertex)
{
	const auto& input = tangent_input(context);
	const auto& value = input.texcoords.at(tangent_source_index(input, face, vertex));
	out[0] = value.x;
	out[1] = value.y;
}

inline void set_tangent(
	const SMikkTSpaceContext* context,
	const float tangent[],
	const float sign,
	const int face,
	const int vertex)
{
	auto& input = tangent_input(context);
	input.corner_tangents.at(static_cast<size_t>(face) * 3 + vertex) =
		glm::vec4(tangent[0], tangent[1], tangent[2], sign);
}

struct TangentRemap
{
	std::vector<uint32_t> source_vertices;
	std::vector<glm::vec4> tangents;
	std::vector<uint32_t> indices;
};

struct TangentVertexKey
{
	uint32_t source_vertex;
	glm::vec4 tangent;
	bool operator==(const TangentVertexKey&) const = default;
};

struct TangentVertexKeyHash
{
	size_t operator()(const TangentVertexKey& key) const
	{
		return std::hash<uint32_t>()(key.source_vertex) ^ std::hash<glm::vec4>()(key.tangent);
	}
};

inline TangentRemap generate_tangents(
	const std::vector<glm::vec3>& positions,
	const std::vector<glm::vec3>& normals,
	const std::vector<glm::vec2>& texcoords,
	const std::vector<uint32_t>& indices)
{
	if (indices.empty() || indices.size() % 3 != 0)
		throw ResourceLoadError("ResourceLoader: tangent generation requires triangle indices");
	if (positions.size() != normals.size() || positions.size() != texcoords.size())
		throw ResourceLoadError("ResourceLoader: tangent-generation attribute counts differ");

	TangentGenerationInput input{ positions, normals, texcoords, indices,
		std::vector<glm::vec4>(indices.size(), glm::vec4(0.0f)) };
	SMikkTSpaceInterface interface{};
	interface.m_getNumFaces = tangent_face_count;
	interface.m_getNumVerticesOfFace = tangent_vertices_per_face;
	interface.m_getPosition = tangent_position;
	interface.m_getNormal = tangent_normal;
	interface.m_getTexCoord = tangent_texcoord;
	interface.m_setTSpaceBasic = set_tangent;
	SMikkTSpaceContext context{ &interface, &input };
	if (!genTangSpaceDefault(&context))
		throw ResourceLoadError("ResourceLoader: MikkTSpace tangent generation failed");

	TangentRemap result;
	result.indices.reserve(indices.size());
	std::unordered_map<TangentVertexKey, uint32_t, TangentVertexKeyHash> remap;
	for (size_t corner = 0; corner < indices.size(); ++corner)
	{
		auto tangent = input.corner_tangents[corner];
		auto tangent_xyz = glm::vec3(tangent);
		if (!std::isfinite(tangent.x) || !std::isfinite(tangent.y) || !std::isfinite(tangent.z) ||
			glm::length(tangent_xyz) < 0.00001f)
		{
			const auto normal = normals.at(indices[corner]);
			if (!std::isfinite(normal.x) || !std::isfinite(normal.y) || !std::isfinite(normal.z) ||
				glm::length(normal) < 0.00001f)
				throw ResourceLoadError("ResourceLoader: cannot generate a tangent from an invalid normal");
			const auto normalized_normal = glm::normalize(normal);
			const auto fallback_axis = std::abs(normalized_normal.x) < 0.9f
				? glm::vec3(1.0f, 0.0f, 0.0f)
				: glm::vec3(0.0f, 1.0f, 0.0f);
			tangent_xyz = glm::cross(fallback_axis, normalized_normal);
		}
		tangent = glm::vec4(glm::normalize(tangent_xyz), tangent.w < 0.0f ? -1.0f : 1.0f);
		const TangentVertexKey key{ indices[corner], tangent };
		auto [it, inserted] = remap.emplace(key, static_cast<uint32_t>(result.source_vertices.size()));
		if (inserted)
		{
			result.source_vertices.push_back(key.source_vertex);
			result.tangents.push_back(key.tangent);
		}
		result.indices.push_back(it->second);
	}
	return result;
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
	const std::vector<glm::vec2>& texcoords,
	const std::vector<glm::vec4>& tangents)
{
	TexVertices vertices(positions.size());
	for (size_t i = 0; i < vertices.size(); ++i)
	{
		vertices[i].pos = positions[i];
		vertices[i].normal = normals[i];
		vertices[i].texCoord = texcoords[i];
		vertices[i].tangent = tangents[i];
	}
	return vertices;
}

inline SkinnedVertices load_skinned_vertices(
	const std::vector<glm::vec3>& positions,
	const std::vector<glm::vec3>& normals,
	const std::vector<glm::vec2>& texcoords,
	const std::vector<glm::vec4>& tangents,
	const std::vector<glm::vec4>& joints,
	const std::vector<glm::vec4>& weights)
{
	SkinnedVertices vertices(positions.size());
	for (size_t i = 0; i < vertices.size(); ++i)
	{
		vertices[i].pos = positions[i];
		vertices[i].normal = normals[i];
		vertices[i].texCoord = texcoords[i];
		vertices[i].tangent = tangents[i];
		vertices[i].bone_ids = joints[i];
		vertices[i].bone_weights = weights[i];
	}
	return vertices;
}

inline TexVertices load_tex_vertices(
	const std::vector<glm::vec3>& positions,
	const std::vector<glm::vec3>& normals,
	const std::vector<glm::vec2>& texcoords,
	const GltfImport::TangentRemap& remap)
{
	TexVertices vertices(remap.source_vertices.size());
	for (size_t i = 0; i < vertices.size(); ++i)
	{
		const auto source = remap.source_vertices[i];
		vertices[i].pos = positions[source];
		vertices[i].normal = normals[source];
		vertices[i].texCoord = texcoords[source];
		vertices[i].tangent = remap.tangents[i];
	}
	return vertices;
}

inline SkinnedVertices load_skinned_vertices(
	const std::vector<glm::vec3>& positions,
	const std::vector<glm::vec3>& normals,
	const std::vector<glm::vec2>& texcoords,
	const GltfImport::TangentRemap& remap,
	const std::vector<glm::vec4>& joints,
	const std::vector<glm::vec4>& weights)
{
	SkinnedVertices vertices(remap.source_vertices.size());
	for (size_t i = 0; i < vertices.size(); ++i)
	{
		const auto source = remap.source_vertices[i];
		vertices[i].pos = positions[source];
		vertices[i].normal = normals[source];
		vertices[i].texCoord = texcoords[source];
		vertices[i].tangent = remap.tangents[i];
		vertices[i].bone_ids = joints[source];
		vertices[i].bone_weights = weights[source];
	}
	return vertices;
}
