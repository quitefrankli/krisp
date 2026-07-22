#include "object.hpp"
#include "serialization/serializer.hpp"
#include "serialization/serialization_helpers.hpp"

#include "maths.hpp"
#include "resource_loader/resource_loader.hpp"
#include "serialization/resource_provenance.hpp"
#include "utility.hpp"

#include <glm/ext.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <quill/LogMacros.h>

#include <iostream>
#include <algorithm>
#include <ranges>
#include <numeric>
#include <limits>


Object::Object(Object&& other) noexcept :
	id(other.id),
	renderables(std::move(other.renderables)),
	children(std::move(other.children)),
	parent(other.parent),
	world_transform(std::move(other.world_transform)),
	relative_transform(std::move(other.relative_transform)),
	name(std::move(other.name)),
	bVisible(other.bVisible),
	aabb(std::move(other.aabb)),
	bounding_sphere(std::move(other.bounding_sphere))
{
	if (parent)
		parent->children[id] = this;
	for (auto& [_, child] : children)
		child->parent = this;
	other.parent = nullptr;
	other.children.clear();
}

Object::~Object()
{
	detach_all_children();
	detach_from();
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

	// in case the parent's transform has changed since we were attached
	sync_world_from_relative();

	// callbacks
	parent->on_child_detached(this);
	on_parent_detached(parent);

	parent->children.erase(get_id());
	parent = nullptr;
}

void Object::attach_to(Object* new_parent)
{
	if (!new_parent)
	{
		detach_from();
		return;
	}

	if (new_parent == this)
	{
		LOG_ERROR(Utility::get_logger(), "ERROR: attempted to attach to itself!");
		return;
	}

	for (const Object* ancestor = new_parent; ancestor; ancestor = ancestor->parent)
	{
		if (ancestor == this)
		{
			LOG_ERROR(Utility::get_logger(), "ERROR: attempted to create cyclic parent-child relationship! Detach first!");
			return;
		}
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

void Object::serialize(Serializer& out) const
{
	out.write("type", serialization_type());
	out.write("id", id.get_underlying());
	out.write("name", name);
	out.write("visible", bVisible);
	Serialization::write_transform(out, "world_transform", get_maths_transform());
	Serialization::write_transform(out, "relative_transform", relative_transform);
	auto bounds = out.map("aabb");
	Serialization::write_vec3(bounds, "min", aabb.min_bound);
	Serialization::write_vec3(bounds, "max", aabb.max_bound);
	if (parent)
		out.write("parent_id", parent->get_id().get_underlying());
	else
		out.write_null("parent_id");
	auto saved_renderables = out.sequence("renderables");
	for (const auto& renderable : renderables)
	{
		auto saved = saved_renderables.append_map();
		if (const auto* origin = ResourceProvenance::mesh(renderable.mesh_id))
		{
			auto source = saved.map("mesh_source");
			source.write("path", origin->source);
			source.write("scene", origin->scene);
			source.write("node", origin->node);
			source.write("primitive", origin->primitive);
		}
		else
			saved.write("mesh_id", renderable.mesh_id.get_underlying());
		auto materials = saved.sequence("material_ids");
		for (const auto material : renderable.material_ids)
		{
			if (const auto* origin = ResourceProvenance::material(material))
			{
				auto source = materials.append_map();
				source.write("path", origin->source);
				source.write("scene", origin->scene);
				source.write("node", origin->node);
				source.write("primitive", origin->primitive);
			}
			else
				materials.append(material.get_underlying());
		}
		saved.write("render_type", static_cast<int>(renderable.pipeline_render_type));
		saved.write("alpha_mode", static_cast<int>(renderable.alpha_mode));
		saved.write("alpha_cutoff", renderable.alpha_cutoff);
		saved.write("opacity", renderable.opacity);
		saved.write("casts_shadow", renderable.casts_shadow);
		saved.write("render_on_top", renderable.render_on_top);
	}
}

void Object::deserialize(const Deserializer& in)
{
	id = ObjectID(in.read<uint64_t>("id"));
	ObjectID::set_next_id(std::max(ObjectID::get_next_id(), id.get_underlying() + 1));
	name = in.read<std::string>("name");
	bVisible = in.read<bool>("visible");
	world_transform = Serialization::read_transform(in, "world_transform");
	relative_transform = Serialization::read_transform(in, "relative_transform");
	const auto bounds = in.child("aabb");
	aabb = AABB(Serialization::read_vec3(bounds, "min"), Serialization::read_vec3(bounds, "max"));
	renderables.clear();
	for (const auto& saved : in.child("renderables").elements())
	{
		Renderable renderable;
		const auto keys = saved.keys();
		const bool imported_mesh = std::ranges::find(keys, "mesh_source") != keys.end();
		renderable.mesh_id = imported_mesh ? MeshID{} : MeshID(saved.read<uint64_t>("mesh_id"));
		for (const auto& material : saved.child("material_ids").elements())
			if (material.kind() == SerializationKind::Scalar)
				renderable.material_ids.emplace_back(material.as<uint64_t>());
		renderable.pipeline_render_type = static_cast<ERenderType>(saved.read<int>("render_type"));
		renderable.alpha_mode = static_cast<EAlphaMode>(saved.read<int>("alpha_mode"));
		renderable.alpha_cutoff = saved.read<float>("alpha_cutoff");
		renderable.opacity = saved.read<float>("opacity");
		renderable.casts_shadow = saved.read<bool>("casts_shadow");
		renderable.render_on_top = saved.read<bool>("render_on_top");
		renderables.push_back(std::move(renderable));
	}
}
