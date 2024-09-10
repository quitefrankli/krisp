#include "object.hpp"

#include "maths.hpp"
#include "resource_loader/resource_loader.hpp"

#include <glm/ext.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include <iostream>
#include <numeric>
#include <limits>


Object::Object(Object&& other) noexcept :
	id(other.id),
	renderables(std::move(other.renderables)),
	children(std::move(other.children)),
	parent(other.parent),
	world_transform(std::move(other.world_transform)),
	relative_transform(std::move(other.relative_transform)),
	bVisible(other.bVisible),
	aabb(std::move(other.aabb)),
	bounding_sphere(std::move(other.bounding_sphere))
{
}

void Object::sync_world_from_relative() const
{
	if (!parent)
	{
		return;
	}

	world_transform.set_mat4(parent->get_transform() * relative_transform.get_mat4());
}

Maths::Transform Object::get_maths_transform() const
{
	sync_world_from_relative();

	return world_transform;
}

glm::mat4 Object::get_transform() const
{
	sync_world_from_relative();

	return world_transform.get_mat4();
}

glm::vec3 Object::get_position() const
{
	sync_world_from_relative();

	return world_transform.get_pos();
}

glm::vec3 Object::get_scale() const
{
	sync_world_from_relative();

	return world_transform.get_scale();
}

glm::quat Object::get_rotation() const
{
	sync_world_from_relative();

	return world_transform.get_orient();
}

void Object::set_transform(const glm::mat4& transform)
{
	sync_world_from_relative();
	world_transform.set_mat4(transform);

	if (parent)
	{
		// also set relative transform if this object is attached so the world
		// transform doesn't get overwritten on next sync
		relative_transform.set_mat4(glm::inverse(parent->get_transform()) * world_transform.get_mat4());
	}
}

void Object::set_position(const glm::vec3& position)
{
	sync_world_from_relative();
	world_transform.set_pos(position);

	if (parent)
	{
		// also set relative transform if this object is attached so the world
		// transform doesn't get overwritten on next sync
		relative_transform.set_mat4(glm::inverse(parent->get_transform()) * world_transform.get_mat4());
	}
}

void Object::set_scale(const glm::vec3& scale)
{
	sync_world_from_relative();
	world_transform.set_scale(scale);

	if (parent)
	{
		// also set relative transform if this object is attached so the world
		// transform doesn't get overwritten on next sync
		relative_transform.set_mat4(glm::inverse(parent->get_transform()) * world_transform.get_mat4());
	}
}

void Object::set_rotation(const glm::quat& rotation)
{
	sync_world_from_relative();
	world_transform.set_orient(rotation);

	if (parent)
	{
		// also set relative transform if this object is attached so the world
		// transform doesn't get overwritten on next sync
		relative_transform.set_mat4(glm::inverse(parent->get_transform()) * world_transform.get_mat4());
	}
}

glm::mat4 Object::get_relative_transform() const
{
	return relative_transform.get_mat4();
}

glm::vec3 Object::get_relative_position() const
{
	return relative_transform.get_pos();
}

glm::vec3 Object::get_relative_scale() const
{
	return relative_transform.get_scale();
}

glm::quat Object::get_relative_rotation() const
{
	return relative_transform.get_orient();
}

void Object::set_relative_transform(const glm::mat4& transform)
{
	relative_transform.set_mat4(transform);
}

void Object::set_relative_position(const glm::vec3& position)
{
	relative_transform.set_pos(position);
}

void Object::set_relative_scale(const glm::vec3& scale)
{
	relative_transform.set_scale(scale);
}

void Object::set_relative_rotation(const glm::quat& rotation)
{
	relative_transform.set_orient(rotation);
}

void Object::detach_from()
{
	if (!parent)
	{
		return;
	}

	// callbacks
	parent->on_child_detached(this);
	on_parent_detached(parent);

	parent->children.erase(get_id());
	parent = nullptr;
}

void Object::attach_to(Object* new_parent)
{
	if (new_parent == this)
	{
		std::cout << "ERROR: attempted to attach to itself!\n";
		return;
	}

	if (new_parent->parent == this)
	{
		std::cout << "ERROR: attempted to attach to parent that is already attached to this object! Please detach first!\n";
		return;
	}

	if (parent)
	{
		if (parent == new_parent) // already attached
		{
			return;
		} else {
			detach_from();
		}
	}

	set_relative_transform(glm::inverse(new_parent->get_transform()) * get_transform());
	
	// callbacks
	on_parent_attached(new_parent);
	new_parent->on_child_attached(this);

	new_parent->children.emplace(get_id(), this);
	parent = new_parent;
}

void Object::detach_all_children()
{
	// can't do simple for loop since we may be removing elements while iterating
	for (auto child = children.begin(), next_child = child; child != children.end(); child = next_child)
	{
		next_child++;
		child->second->detach_from();
	}
}

bool Object::check_collision(const Maths::Ray& ray)
{
	if (!get_visibility())
		return false;

	const float radius = 0.5f; // this doesn't really work for non-unit objects, but will leave for now
	return Maths::check_spherical_collision(ray, Maths::Sphere(get_position(), radius));
}

bool Object::check_collision(const Maths::Ray& ray, glm::vec3& intersection) const
{
	// assume unit sphere
	const std::optional<glm::vec3> x = Maths::ray_sphere_collision(
		Maths::Sphere(get_position(), 1.0f), ray);
	if (x)
	{
		intersection = x.value();
		return true;
	} else 
	{
		return false;
	}
}