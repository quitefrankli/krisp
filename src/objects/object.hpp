#pragma once

#include "shapes/shape.hpp"
#include "maths.hpp"

#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>

#include <vector>


class GraphicsEngineObject;
class ResourceLoader;

class ObjectAbstract
{
public:
	ObjectAbstract(const ObjectAbstract& object) = delete;
	ObjectAbstract& operator=(const ObjectAbstract& object) = delete;

	ObjectAbstract();
	ObjectAbstract(uint64_t id);
	ObjectAbstract(ObjectAbstract&& object) noexcept = default;
	~ObjectAbstract() = default;

	uint64_t get_id() const { return id; }
	// void set_id(uint64_t id) { this->id = id; } ;// we don't really want to set id ever
	void generate_new_id() { id = global_id++; }

private:
	static uint64_t global_id;
	uint64_t id;
};

class Object : public ObjectAbstract
{
public:
	Object(const Object& object) = delete;

	Object() = default;
	Object(Object&& object) noexcept = default;
	Object(
		ResourceLoader& loader, 
		const std::string& mesh, 
		const std::string& texture, 
		const glm::mat4& transform = glm::mat4(1.0f));
	Object(std::string texture);

	std::vector<Shape> shapes;
	// when using indexed draws, the number of unique vertices < number of indices
	uint32_t get_num_unique_vertices() const;
	uint32_t get_num_vertex_indices() const;

	std::string texture;

	void toggle_visibility() { bVisible = !bVisible; }
	bool get_visibility() const { return bVisible; }

public:
	virtual glm::mat4 get_transform() const;
	virtual glm::vec3 get_position() const { return position; }
	virtual glm::vec3 get_scale() const { return scale; }
	virtual glm::quat get_rotation() const { return orientation; }

	virtual void set_transform(const glm::mat4& transform);
	virtual void set_position(const glm::vec3& position);
	virtual void set_scale(const glm::vec3& scale);
	virtual void set_rotation(const glm::quat& rotation);

private:
	// void calculate_shape_extent();
	void calculate_shape_extent_sphere();

private:
	mutable bool is_transform_old = true;
	mutable glm::mat4 cached_transform;
	glm::vec3 position = glm::vec3(0.f);
	glm::vec3 scale = glm::vec3(1.f);
	glm::quat orientation; // default init creates identity quaternion
	bool bVisible = true;

//
// collision
//

private:
	template<typename geometry>
	void calculate_bounding_primitive();
	Maths::Sphere bounding_primitive_sphere;
	bool is_bounding_primitive_cached = false;

public:
	template<typename geometry>
 	bool check_collision(Maths::Ray& ray);
};