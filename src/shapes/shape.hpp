#pragma once

#include "shared_data_structures.hpp"
#include "collision/bounding_box.hpp"
#include "materials/materials.hpp"
#include "collision/bounding_box.hpp"
#include "shape_id.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>


namespace Maths
{
	struct Ray;
}

// abstract shape
class Shape
{
public:
	template<typename DerivedT>
	DerivedT& as()
	{
		// throws exception on failure
		return dynamic_cast<DerivedT&>(*this);
	}

	Shape() : id(global_id++) {}
	virtual ~Shape() = default;

	std::vector<uint32_t>& get_indices() { return indices; }
	void set_indices(std::vector<uint32_t>&& indices) { this->indices = std::move(indices); }

	virtual uint32_t get_num_unique_vertices() const = 0;
	virtual uint32_t get_num_vertex_indices() const { return static_cast<uint32_t>(indices.size()); };

	void set_material(const Material& material) { this->material = material; }
	const Material& get_material() const { return material; }
	ShapeID get_id() const { return id; }

	virtual void generate_normals() = 0;

	virtual const std::byte* get_vertices_data() const = 0;
	const std::byte* get_indices_data() const { return reinterpret_cast<const std::byte*>(indices.data()); }

	virtual size_t get_vertices_data_size() const = 0;
	size_t get_indices_data_size() const { return indices.size() * sizeof(uint32_t); }

	virtual void transform_vertices(const glm::mat4& transform) = 0;
	virtual void transform_vertices(const glm::quat& quat) = 0;
	virtual void translate_vertices(const glm::vec3& vec) = 0;
	virtual AABB calculate_bounding_box() const = 0;

public: // static functions
	template<typename VertexType>
	static void transform_vertices(std::vector<VertexType>& vertices, const glm::mat4& transform);
	template<typename VertexType>
	static void transform_vertices(std::vector<VertexType>& vertices, const glm::quat& quat);
	template<typename VertexType>
	static void translate_vertices(std::vector<VertexType>& vertices, const glm::vec3& vec);
	template<typename VertexType>
	static void generate_normals(std::vector<VertexType>& vertices, std::vector<uint32_t>& indices);
	template<typename VertexType>
	static AABB calculate_bounding_box(const std::vector<VertexType>& vertices);

protected:
	// WARNING this is expensive
	// void deduplicate_vertices();

	Material material;
	std::vector<uint32_t> indices;
	
private:
	const ShapeID id;
	static ShapeID global_id;
};

using ShapePtr = std::unique_ptr<Shape>;

template<typename vertex_t>
class DerivedShape : public Shape
{
public:
	using VertexType = vertex_t;

	DerivedShape() = default;
	DerivedShape(const DerivedShape& shape) = delete;
	DerivedShape& operator=(const DerivedShape& shape) = default;
	DerivedShape(DerivedShape&& shape) noexcept = default;

	uint32_t get_num_unique_vertices() const override { return static_cast<uint32_t>(vertices.size()); }
	virtual void transform_vertices(const glm::mat4& transform) override { Shape::transform_vertices(vertices, transform); }
	virtual void transform_vertices(const glm::quat& quat) override { Shape::transform_vertices(vertices, quat); }
	virtual void translate_vertices(const glm::vec3& vec) override { Shape::translate_vertices(vertices, vec); }
	virtual void generate_normals() override { Shape::generate_normals(vertices, indices); }
	std::vector<VertexType>& get_vertices() { return vertices; }
	void set_vertices(std::vector<VertexType>&& vertices) { this->vertices = std::move(vertices); }
	virtual const std::byte* get_vertices_data() const override { return reinterpret_cast<const std::byte*>(vertices.data()); }
	virtual size_t get_vertices_data_size() const override { return vertices.size() * sizeof(VertexType); }
	virtual AABB calculate_bounding_box() const override { return Shape::calculate_bounding_box(vertices); }

private:
	std::vector<VertexType> vertices;
};

using ColorShape = DerivedShape<SDS::ColorVertex>;
using TexShape = DerivedShape<SDS::TexVertex>;
using SkinnedShape = DerivedShape<SDS::SkinnedVertex>;