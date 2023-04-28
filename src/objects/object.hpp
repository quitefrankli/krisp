#pragma once

#include "graphics_engine/pipeline/pipeline_types.hpp" // TODO: come up with a to decouiple this, having graphics engine code here is bad
#include "shapes/shape.hpp"
#include "maths.hpp"
#include "collision/bounding_box.hpp"
#include "object_id.hpp"
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>

#include <vector>
#include <map>


class ResourceLoader;
class ObjectAbstract
{
public:
	ObjectAbstract(const ObjectAbstract& object) = delete;
	ObjectAbstract& operator=(const ObjectAbstract& object) = delete;

	ObjectAbstract();
	ObjectAbstract(ObjectID id);
	ObjectAbstract(ObjectAbstract&& object) noexcept = default;
	virtual ~ObjectAbstract() = default;

	ObjectID get_id() const { return id; }

public: // getters and setters
	const std::string& get_name() const { return name; }
	void set_name(const std::string& name) { this->name = name; }
	void set_name(const std::string_view name) { this->name = name; }

private:
	std::string name;
	static ObjectID global_id;
	ObjectID id;
};

class Object : public ObjectAbstract
{
public:
	Object() = default;
	Object(std::unique_ptr<Shape>&& shape)
	{
		shapes.push_back(std::move(shape));
	}
	Object(const Object& object) = delete;
	Object(Object&& object) noexcept;
	virtual ~Object() override;

	// when using indexed draws, the number of unique vertices < number of indices
	uint32_t get_num_unique_vertices() const;
	uint32_t get_num_vertex_indices() const;

	size_t get_vertices_data_size() const;
	size_t get_indices_data_size() const;

	virtual EPipelineType get_render_type() const { return render_type; }
	void set_render_type(EPipelineType type) { render_type = type; }

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
	// world
	virtual glm::mat4 get_transform() const;
	virtual glm::vec3 get_position() const;
	virtual glm::vec3 get_scale() const;
	virtual glm::quat get_rotation() const;

	virtual void set_transform(const glm::mat4& transform);
	virtual void set_position(const glm::vec3& position);
	virtual void set_scale(const glm::vec3& scale);
	virtual void set_rotation(const glm::quat& rotation);

	// relative
	virtual glm::mat4 get_relative_transform() const;
	virtual glm::vec3 get_relative_position() const;
	virtual glm::vec3 get_relative_scale() const;
	virtual glm::quat get_relative_rotation() const;

	virtual void set_relative_transform(const glm::mat4& transform);
	virtual void set_relative_position(const glm::vec3& position);
	virtual void set_relative_scale(const glm::vec3& scale);
	virtual void set_relative_rotation(const glm::quat& rotation);

	std::vector<std::unique_ptr<Shape>>& get_shapes() { return shapes; }
	const std::vector<std::unique_ptr<Shape>>& get_shapes() const { return shapes; }
	AABB get_aabb() const { return aabb; }
	void set_aabb(const AABB& aabb) { this->aabb = aabb; }

protected:
	std::map<ObjectID, Object*> children;
	Object* parent = nullptr;
	std::vector<std::unique_ptr<Shape>> shapes;

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
	// there's a lot of caching going on with the below 2 transforms hence the mutable
	mutable Maths::Transform world_transform;
	mutable Maths::Transform relative_transform; // as opposed to world, relative SHOULD ALWAYS be up to date
	// when relative_transform updates then world transform will be outdated
	// mutable bool bIsWorldTransformOld = false;
	void sync_world_from_relative() const;

	AABB aabb;
	Maths::Sphere bounding_sphere;

	bool bVisible = true;

	EPipelineType render_type = EPipelineType::COLOR;

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
	virtual bool check_collision(const Maths::Ray& ray, glm::vec3& intersection) const;
};