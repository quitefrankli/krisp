#pragma once

#include "render_types.hpp"
#include "shapes/shape.hpp"
#include "maths.hpp"
#include "collision/bounding_box.hpp"

#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>

#include <vector>
#include <map>


class GraphicsEngineObject;
class ResourceLoader;

using obj_id_t = uint64_t;

class ObjectAbstract
{
public:
	ObjectAbstract(const ObjectAbstract& object) = delete;
	ObjectAbstract& operator=(const ObjectAbstract& object) = delete;

	ObjectAbstract();
	ObjectAbstract(obj_id_t id);
	ObjectAbstract(ObjectAbstract&& object) noexcept = default;
	virtual ~ObjectAbstract() = default;

	obj_id_t get_id() const { return id; }
	void generate_new_id() { id = global_id++; }

public: // getters and setters
	const std::string& get_name() const { return name; }
	void set_name(const std::string& name) { this->name = name; }
	void set_name(const std::string_view name) { this->name = name; }

private:
	std::string name;
	static obj_id_t global_id;
	obj_id_t id;
};

class Object : public ObjectAbstract
{
public:
	Object(const Object& object) = delete;

	Object() = default;
	Object(Object&& object) noexcept;
	virtual ~Object() override;

	std::vector<Shape> shapes;
	// when using indexed draws, the number of unique vertices < number of indices
	uint32_t get_num_unique_vertices() const;
	uint32_t get_num_vertex_indices() const;

	virtual ERenderType get_render_type() const { return render_type; }
	void set_render_type(ERenderType type) { render_type = type; }

	virtual void toggle_visibility() { bVisible = !bVisible; }
	virtual void set_visibility(bool isVisible) { bVisible = isVisible; }
	bool get_visibility() const { return bVisible; }

	// detach from parent
	virtual void detach_from();
	// attach to parent
	virtual void attach_to(Object* parent);
	// detach all children
	virtual void detach_all_children();

public:
	virtual glm::mat4 get_transform() const;
	virtual glm::vec3 get_position() const { return transformation_components.position; }
	virtual glm::vec3 get_scale() const { return transformation_components.scale; }
	virtual glm::quat get_rotation() const { return transformation_components.orientation; }
	virtual const Maths::TransformationComponents& get_transformation_components() const { return transformation_components; }

	const std::vector<Shape>& get_shapes() const { return shapes; }
	AABB get_aabb() const { return aabb; }
	void set_aabb(const AABB& aabb) { this->aabb = aabb; }

	virtual void set_transform(const glm::mat4& transform);
	virtual void set_position(const glm::vec3& position);
	virtual void set_scale(const glm::vec3& scale);
	virtual void set_rotation(const glm::quat& rotation);
	virtual void set_transformation_components(const Maths::TransformationComponents& components);

protected:
	std::map<uint64_t, Object*> children;
	Object* parent = nullptr;

	// callback when a child gets attached
	virtual void on_child_attached(Object* new_child) {}
	// callback when a parent gets attached
	virtual void on_parent_attached(Object* new_parent) {}
	// callback when a child gets deattached
	virtual void on_child_detached(Object* old_child) {}
	// callback when a parent gets deattached
	virtual void on_parent_detached(Object* old_parent) {}

private:
	// void calculate_shape_extent();
	void calculate_shape_extent_sphere();

private:
	mutable bool is_transform_old = true;
	mutable glm::mat4 cached_transform;
	Maths::TransformationComponents transformation_components;
	AABB aabb;
	Maths::Sphere bounding_sphere;

	// not really necessary but it's nice to have for easy access
	glm::vec3& position = transformation_components.position;
	glm::vec3& scale = transformation_components.scale;
	glm::quat& orientation = transformation_components.orientation;
	bool bVisible = true;

	ERenderType render_type = ERenderType::COLOR;

//
// collision
//

protected:
	template<typename geometry>
	void calculate_bounding_primitive();
	Maths::Sphere bounding_primitive_sphere;
	bool is_bounding_primitive_cached = false;

public:
 	virtual bool check_collision(const Maths::Ray& ray);
};