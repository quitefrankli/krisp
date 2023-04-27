#pragma once

#include "shared_data_structures.hpp"
#include "collision/bounding_box.hpp"
#include "materials/materials.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>


namespace Maths
{
	struct Ray;
}

struct ShapeID
{
public:
	explicit ShapeID(uint64_t id) : id(id) {}

	bool operator==(const ShapeID& other) const { return id == other.id; }
	bool operator!=(const ShapeID& other) const { return id != other.id; }
	bool operator<(const ShapeID& other) const { return id < other.id; }
	bool operator>(const ShapeID& other) const { return id > other.id; }

	ShapeID operator++() { return ShapeID(++id); }
	ShapeID operator++(int) { return ShapeID(id++); }

	uint64_t get_underlying() const { return id; }

private:
	uint64_t id;
};

// class AbsShape
// {
// public:
// 	template<typename DerivedT>
// 	DerivedT& as()
// 	{
// 		return dynamic_cast<DerivedT&>(*this);
// 	}

// 	Shape() : id(global_id++) {}
// 	virtual ~Shape() = default;

// 	Shape(const Shape& shape) = delete;
// 	Shape& operator=(const Shape& shape) = delete;
// 	Shape(Shape&& shape) noexcept = default;

// 	std::vector<SDS::Vertex> vertices;
// 	std::vector<uint32_t> indices;
	
// 	uint32_t get_num_unique_vertices() const { return static_cast<uint32_t>(vertices.size()); }
// 	uint32_t get_num_vertex_indices() const { return static_cast<uint32_t>(indices.size()); };
// 	void transform_vertices(const glm::mat4& transform);
// 	void transform_vertices(const glm::quat& quat);
// 	void translate_vertices(const glm::vec3& vec);
// 	void generate_normals();

// 	void set_material(const Material& material) { this->material = material; }
// 	const Material& get_material() const { return material; }

// 	// WARNING this is expensive
// 	void deduplicate_vertices();

// 	virtual bool check_collision(const Maths::Ray& ray);
// 	AABB aabb;

// 	ShapeID get_id() const { return id; }

// protected:
// 	Material material;
	
// private:
// 	const ShapeID id;
// 	static ShapeID global_id;
// };

// class ColorShape
// {

// };

class Shape
{
public:
	Shape() : id(global_id++) {}
	virtual ~Shape() = default;

	Shape(const Shape& shape) = delete;
	Shape& operator=(const Shape& shape) = delete;
	Shape(Shape&& shape) noexcept = default;

	std::vector<SDS::Vertex> vertices;
	std::vector<uint32_t> indices;
	
	uint32_t get_num_unique_vertices() const { return static_cast<uint32_t>(vertices.size()); }
	uint32_t get_num_vertex_indices() const { return static_cast<uint32_t>(indices.size()); };
	void transform_vertices(const glm::mat4& transform);
	void transform_vertices(const glm::quat& quat);
	void translate_vertices(const glm::vec3& vec);
	void generate_normals();

	void set_material(const Material& material) { this->material = material; }
	const Material& get_material() const { return material; }

	// WARNING this is expensive
	void deduplicate_vertices();

	virtual bool check_collision(const Maths::Ray& ray);
	AABB aabb;

	ShapeID get_id() const { return id; }

protected:
	Material material;
	
private:
	const ShapeID id;

	static ShapeID global_id;
};