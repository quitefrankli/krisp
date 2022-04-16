#include "object.hpp"

#include "maths.hpp"
#include "resource_loader.hpp"

#include <glm/ext.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include <iostream>
#include <numeric>
#include <limits>


uint64_t ObjectAbstract::global_id = 0;

ObjectAbstract::ObjectAbstract()
{
	generate_new_id();
}

ObjectAbstract::ObjectAbstract(uint64_t id)
{
	this->id = id;
}

Object::Object(Object&& other) noexcept :
	ObjectAbstract(std::move(other)),
	shapes(std::move(other.shapes)),
	children(std::move(children)),
	parent(other.parent),
	is_transform_old(other.is_transform_old),
	cached_transform(std::move(other.cached_transform)),
	transformation_components(std::move(other.transformation_components)),
	bVisible(other.bVisible),
	render_type(other.render_type),
	aabb(std::move(other.aabb)),
	bounding_sphere(std::move(other.bounding_sphere))
{
}

uint32_t Object::get_num_unique_vertices() const
{
	return std::accumulate(shapes.begin(), shapes.end(), 0, [](uint32_t total, const auto& shape){ 
		return total + shape.get_num_unique_vertices(); });
}

uint32_t Object::get_num_vertex_indices() const
{
	return std::accumulate(shapes.begin(), shapes.end(), 0, [](uint32_t total, const auto& shape){ 
		return total + shape.get_num_vertex_indices(); });
}

void Object::set_position(const glm::vec3& position)
{
	is_transform_old = true;

	glm::vec3 diff = position - this->position;

	this->position = position;
	for (auto& child : children)
	{
		child.second->set_position(child.second->get_position() + diff);
	}
}

void Object::set_scale(const glm::vec3& scale)
{
	is_transform_old = true;
	this->scale = scale;
}

void Object::set_rotation(const glm::quat& rotation)
{
	is_transform_old = true;

	if (!children.empty())
	{
		glm::quat diff = glm::normalize(rotation * glm::conjugate(orientation));
		for (auto& child : children)
		{
			// break;
			// rotate the child about its own axis
			// child.second->set_rotation(diff * child.second->get_rotation());

			// in addition to the rotation of the child on its own axis
			// we also need to rotate the child about the parent

			// for rotating about arbitrary axis, we need to:
			// 1. move it to the origin (relative to its parent)
			glm::vec3 temp_pos = child.second->get_position() - get_position();

			// 2. apply rotation
			temp_pos = diff * temp_pos;

			// 3. move it back to its location
			child.second->set_position(temp_pos + get_position());

			// 4. also apply rotation (in world space) to the child object
			child.second->set_rotation(diff * child.second->get_rotation());
		}
	}

	orientation = rotation;
}

void Object::set_transformation_components(const Maths::TransformationComponents& components)
{
	transformation_components = components;
	is_transform_old = true;
}

glm::mat4 Object::get_transform() const
{
	if (is_transform_old)
	{
		is_transform_old = false;

		cached_transform = glm::mat4_cast(glm::normalize(orientation)) * glm::scale(scale);
		cached_transform[3] = glm::vec4(position, 1.0f);
	}

	return cached_transform;
}

void Object::set_transform(const glm::mat4& transform)
{
	is_transform_old = false;
	cached_transform = transform;

	auto get_vec3_len = [](const glm::vec4& vec4)
	{
		return std::sqrtf(vec4[0] * vec4[0] + vec4[1] * vec4[1] + vec4[2] * vec4[2]);
	};

	// https://math.stackexchange.com/questions/237369/given-this-transformation-matrix-how-do-i-decompose-it-into-translation-rotati/417813
	orientation = glm::normalize(glm::quat_cast(transform));
	scale[0] = get_vec3_len(transform[0]);
	scale[1] = get_vec3_len(transform[1]);
	scale[2] = get_vec3_len(transform[2]);
	position = transform[3];
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

template<>
void Object::calculate_bounding_primitive<Maths::Sphere>()
{
	float left = std::numeric_limits<float>::max();
	float down = std::numeric_limits<float>::max();
	float in = std::numeric_limits<float>::max();
	float right = -std::numeric_limits<float>::max();
	float up = -std::numeric_limits<float>::max();
	float out = -std::numeric_limits<float>::max();

	for (Shape& shape : shapes)
	{
		for (Vertex& vertex : shape.vertices)
		{
			auto& p = vertex.pos;
			left = std::min<float>(left, p.x);
			down = std::min<float>(down, p.y);
			in = std::min<float>(in, p.z);

			right = std::max<float>(right, p.x);
			up = std::max<float>(up, p.y);
			out = std::max<float>(out, p.z);
		}
	}

	glm::vec3 min_bound(left, down, in);
	glm::vec3 max_bound(right, up, out);
	bounding_primitive_sphere.origin = (min_bound + max_bound) / 2.0f;
	bounding_primitive_sphere.radius = glm::compMax(glm::abs(max_bound - min_bound))/2.0f;

	is_bounding_primitive_cached = true;
}

bool Object::check_collision(const Maths::Ray& ray)
{
	if (!get_visibility())
		return false;

	const float radius = 0.5f; // this doesn't really work for non-unit objects, but will leave for now
	return Maths::check_spherical_collision(ray, Maths::Sphere(get_position(), radius));

	// assert(is_bounding_primitive_cached);

	// // check fast spherical collision
	// float world_radius = bounding_primitive_sphere.radius * glm::compMax(get_scale());
	// glm::vec3 world_origin = get_rotation() * bounding_primitive_sphere.origin * get_scale() + get_position();

	// glm::vec3 projP = -glm::dot(ray.origin, ray.direction) * ray.direction + 
	// 	ray.origin + glm::dot(ray.direction, world_origin) * ray.direction;
	// if (glm::distance(projP, world_origin) > world_radius)
	// {
	// 	return false;
	// }

	// // check proper collision
	// if (shapes.size() > 10)
	// {
	// 	std::cout << "WARNING, too many shapes for collision detection, defaulting to true\n";
	// 	return true;
	// }
	// for (auto& shape : shapes)
	// {
	// 	if (shape.check_collision(ray))
	// 	{
	// 		return true;
	// 	}
	// }
	
	// return false;
}

bool Object::check_collision(const Maths::Ray& ray, glm::vec3& intersection) const
{
	return false;
}

Object::~Object()
{
}