#pragma once

#include "shared_data_structures.hpp"
#include "identifications.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <memory>


struct Mesh 
{
public:
	virtual ~Mesh() = default;
	MeshID get_id() const { return id; }

	const std::vector<uint32_t>& get_indices() const { return indices; }
	void set_indices(std::vector<uint32_t>&& indices) { this->indices = std::move(indices); }

	virtual uint32_t get_num_unique_vertices() const = 0;
	virtual uint32_t get_num_vertex_indices() const { return static_cast<uint32_t>(indices.size()); };

	virtual const std::byte* get_vertices_data() const = 0;
	const std::byte* get_indices_data() const { return reinterpret_cast<const std::byte*>(indices.data()); }
	virtual size_t get_vertices_data_size() const = 0;
	size_t get_indices_data_size() const { return indices.size() * sizeof(uint32_t); }

protected:
	std::vector<uint32_t> indices;

private:
	const MeshID id = MeshID::generate_new_id();
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
	}
	DerivedMesh(std::vector<VertexType_>&& vertices, std::vector<uint32_t>&& indices) : 
		vertices(std::move(vertices))
	{
		this->indices = std::move(indices);
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