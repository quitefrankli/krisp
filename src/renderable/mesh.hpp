#pragma once

#include "shared_data_structures.hpp"
#include "identifications.hpp"
#include "collision/mesh_bvh.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <memory>
#include <algorithm>
#include <cmath>
#include <ranges>


struct Mesh 
{
public:
	virtual ~Mesh() = default;
	MeshID get_id() const { return id; }

	const std::vector<uint32_t>& get_indices() const { return indices; }
	void set_indices(std::vector<uint32_t>&& indices)
	{
		this->indices = std::move(indices);
		pick_data = MeshPickData(pick_data.get_positions(), this->indices);
	}

	virtual uint32_t get_num_unique_vertices() const = 0;
	virtual uint32_t get_num_vertex_indices() const { return static_cast<uint32_t>(indices.size()); };

	virtual const std::byte* get_vertices_data() const = 0;
	const std::byte* get_indices_data() const { return reinterpret_cast<const std::byte*>(indices.data()); }
	virtual size_t get_vertices_data_size() const = 0;
	size_t get_indices_data_size() const { return indices.size() * sizeof(uint32_t); }
	const MeshPickData& get_pick_data() const { return pick_data; }

protected:
	std::vector<uint32_t> indices;

	template<typename VertexType>
	void set_pick_vertices(const std::vector<VertexType>& vertices)
	{
		std::vector<glm::vec3> positions;
		positions.reserve(vertices.size());
		for (const auto& vertex : vertices)
			positions.push_back(vertex.pos);
		pick_data = MeshPickData(std::move(positions), indices);
	}

private:
	const MeshID id = MeshID::generate_new_id();
	MeshPickData pick_data;
};

template<typename VertexType_>
struct DerivedMesh : public Mesh
{
public:
	using VertexType = VertexType_;

	// DerivedMesh() = default;
	DerivedMesh(const std::vector<VertexType_>& vertices, const std::vector<uint32_t>& indices) : 
		vertices(vertices)
	{
		this->indices = indices;
		this->set_pick_vertices(this->vertices);
	}
	DerivedMesh(std::vector<VertexType_>&& vertices, std::vector<uint32_t>&& indices) : 
		vertices(std::move(vertices))
	{
		this->indices = std::move(indices);
		this->set_pick_vertices(this->vertices);
	}
	DerivedMesh(const DerivedMesh& mesh) = delete;
	DerivedMesh& operator=(const DerivedMesh& mesh) = default;
	DerivedMesh(DerivedMesh&& mesh) noexcept = default;

	virtual uint32_t get_num_unique_vertices() const override { return static_cast<uint32_t>(vertices.size()); }
	const std::vector<VertexType_>& get_vertices() const { return vertices; }
	virtual const std::byte* get_vertices_data() const override { return reinterpret_cast<const std::byte*>(vertices.data()); }
	virtual size_t get_vertices_data_size() const override { return vertices.size() * sizeof(VertexType_); }

private:
	std::vector<VertexType_> vertices;
};

using ColorMesh = DerivedMesh<SDS::ColorVertex>;
using TexMesh = DerivedMesh<SDS::TexVertex>;
using SkinnedMesh = DerivedMesh<SDS::SkinnedVertex>;

using ColorVertices = std::vector<SDS::ColorVertex>;
using TexVertices = std::vector<SDS::TexVertex>;
using SkinnedVertices = std::vector<SDS::SkinnedVertex>;
using VertexIndices = std::vector<uint32_t>;

using MeshPtr = std::unique_ptr<Mesh>;

inline bool supports_tangent_space_normal_mapping(const Mesh& mesh)
{
	const auto valid_vertices = []<typename MeshType>(const MeshType& typed_mesh)
	{
		return !typed_mesh.get_vertices().empty()
			&& std::ranges::all_of(typed_mesh.get_vertices(), [](const auto& vertex)
			{
				const auto tangent = glm::vec3(vertex.tangent);
				const auto normal = vertex.normal;
				return std::isfinite(vertex.tangent.x) && std::isfinite(vertex.tangent.y)
					&& std::isfinite(vertex.tangent.z) && std::isfinite(vertex.tangent.w)
					&& std::isfinite(normal.x) && std::isfinite(normal.y)
					&& std::isfinite(normal.z) && glm::length(tangent) > 0.00001f
					&& glm::length(glm::cross(tangent, normal)) > 0.00001f
					&& std::abs(std::abs(vertex.tangent.w) - 1.0f) <= 0.001f;
			});
	};
	if (const auto* textured = dynamic_cast<const TexMesh*>(&mesh))
		return valid_vertices(*textured);
	if (const auto* skinned = dynamic_cast<const SkinnedMesh*>(&mesh))
		return valid_vertices(*skinned);
	return false;
}
