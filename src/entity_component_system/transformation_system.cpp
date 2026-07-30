#include "transformation_system.hpp"

#include "serialization/serialization_helpers.hpp"
#include "serialization/serializer.hpp"

#include <algorithm>
#include <ranges>
#include <stdexcept>
#include <string>
#include <vector>


namespace
{
std::string missing_transformation_message(const EntityID id)
{
	return "TransformationSystem: entity " + std::to_string(id.get_underlying())
		+ " has no transformation";
}

std::string transformation_path(const std::size_t index, const std::string_view field)
{
	return "$.transformation_system[" + std::to_string(index) + "]." + std::string(field);
}
}

Maths::Transform TransformationComponent::get_maths_transform() const
{
	return owner->get_maths_transform(entity_id);
}

glm::mat4 TransformationComponent::get_transform() const
{
	return owner->get_transform(entity_id);
}

glm::vec3 TransformationComponent::get_position() const
{
	return owner->get_position(entity_id);
}

glm::vec3 TransformationComponent::get_scale() const
{
	return owner->get_scale(entity_id);
}

glm::quat TransformationComponent::get_rotation() const
{
	return owner->get_rotation(entity_id);
}

glm::mat4 TransformationComponent::get_relative_transform() const
{
	return owner->get_relative_transform(entity_id);
}

glm::vec3 TransformationComponent::get_relative_position() const
{
	return owner->get_relative_position(entity_id);
}

glm::vec3 TransformationComponent::get_relative_scale() const
{
	return owner->get_relative_scale(entity_id);
}

glm::quat TransformationComponent::get_relative_rotation() const
{
	return owner->get_relative_rotation(entity_id);
}

std::optional<EntityID> TransformationComponent::get_parent_id() const
{
	return owner->get_parent_id(entity_id);
}

void TransformationComponent::set_transform(const glm::mat4& transform)
{
	owner->set_transform(get_entity_id(), transform);
}

void TransformationComponent::set_position(const glm::vec3& position)
{
	owner->set_position(get_entity_id(), position);
}

void TransformationComponent::set_scale(const float uniform_scale)
{
	owner->set_scale(get_entity_id(), uniform_scale);
}

void TransformationComponent::set_scale(const glm::vec3& scale)
{
	owner->set_scale(get_entity_id(), scale);
}

void TransformationComponent::set_rotation(const glm::quat& rotation)
{
	owner->set_rotation(get_entity_id(), rotation);
}

void TransformationComponent::set_relative_transform(const glm::mat4& transform)
{
	owner->set_relative_transform(get_entity_id(), transform);
}

void TransformationComponent::set_relative_position(const glm::vec3& position)
{
	owner->set_relative_position(get_entity_id(), position);
}

void TransformationComponent::set_relative_scale(const glm::vec3& scale)
{
	owner->set_relative_scale(get_entity_id(), scale);
}

void TransformationComponent::set_relative_rotation(const glm::quat& rotation)
{
	owner->set_relative_rotation(get_entity_id(), rotation);
}

bool TransformationComponent::attach_to(const EntityID parent)
{
	return owner->attach_to(get_entity_id(), parent);
}

bool TransformationComponent::attach_to(const TransformationComponent& parent)
{
	if (parent.owner != owner)
		throw std::invalid_argument(
			"TransformationComponent::attach_to: components belong to different systems");
	return attach_to(parent.get_entity_id());
}

bool TransformationComponent::detach_from()
{
	return owner->detach_from(get_entity_id());
}

void TransformationComponent::detach_all_children()
{
	owner->detach_all_children(get_entity_id());
}

TransformationSystem::TransformationSystem(TransformationSystem&& other) noexcept :
	components(std::move(other.components))
{
	rebind_components();
}

TransformationSystem& TransformationSystem::operator=(TransformationSystem&& other) noexcept
{
	if (this == &other)
		return *this;
	components = std::move(other.components);
	rebind_components();
	return *this;
}

void TransformationSystem::rebind_components()
{
	for (auto& transform : components | std::views::values)
		transform.owner = this;
}

void TransformationSystem::add_transformation(
	const EntityID id,
	const TransformationPersistence persistence)
{
	components.try_emplace(
		id,
		TransformationComponent::ConstructionKey{},
		*this,
		id,
		persistence);
}

TransformationComponent& TransformationSystem::get_transformation(const EntityID id)
{
	return component(id);
}

const TransformationComponent& TransformationSystem::get_transformation(const EntityID id) const
{
	return component(id);
}

TransformationComponent& TransformationSystem::component(const EntityID id)
{
	const auto found = components.find(id);
	if (found == components.end())
		throw std::runtime_error(missing_transformation_message(id));
	return found->second;
}

const TransformationComponent& TransformationSystem::component(const EntityID id) const
{
	const auto found = components.find(id);
	if (found == components.end())
		throw std::runtime_error(missing_transformation_message(id));
	return found->second;
}

const Maths::Transform& TransformationSystem::synced_world_transform(const EntityID id) const
{
	auto& transform = components.at(id);
	if (!transform.world_dirty)
		return transform.world_transform;

	if (transform.parent)
		transform.world_transform.set_mat4(
			synced_world_transform(*transform.parent).get_mat4()
			* transform.local_transform.get_mat4());
	else
		transform.world_transform = transform.local_transform;
	transform.world_dirty = false;
	return transform.world_transform;
}

void TransformationSystem::invalidate(const EntityID id)
{
	auto& transform = component(id);
	transform.world_dirty = true;
	for (const auto child : transform.children)
		invalidate(child);
}

void TransformationSystem::invalidate_children(const EntityID id)
{
	for (const auto child : component(id).children)
		invalidate(child);
}

Maths::Transform TransformationSystem::get_maths_transform(const EntityID id) const
{
	component(id);
	return synced_world_transform(id);
}

glm::mat4 TransformationSystem::get_transform(const EntityID id) const
{
	component(id);
	return synced_world_transform(id).get_mat4();
}

glm::vec3 TransformationSystem::get_position(const EntityID id) const
{
	component(id);
	return synced_world_transform(id).get_pos();
}

glm::vec3 TransformationSystem::get_scale(const EntityID id) const
{
	component(id);
	return synced_world_transform(id).get_scale();
}

glm::quat TransformationSystem::get_rotation(const EntityID id) const
{
	component(id);
	return synced_world_transform(id).get_orient();
}

void TransformationSystem::set_transform(const EntityID id, const glm::mat4& value)
{
	auto& transform = component(id);
	transform.world_transform.set_mat4(value);
	transform.local_transform.set_mat4(transform.parent
		? glm::inverse(get_transform(*transform.parent)) * value
		: value);
	transform.world_dirty = false;
	invalidate_children(id);
}

void TransformationSystem::set_position(const EntityID id, const glm::vec3& value)
{
	auto world = get_maths_transform(id);
	world.set_pos(value);
	set_transform(id, world.get_mat4());
}

void TransformationSystem::set_scale(const EntityID id, const glm::vec3& value)
{
	auto world = get_maths_transform(id);
	world.set_scale(value);
	set_transform(id, world.get_mat4());
}

void TransformationSystem::set_rotation(const EntityID id, const glm::quat& value)
{
	auto world = get_maths_transform(id);
	world.set_orient(value);
	set_transform(id, world.get_mat4());
}

glm::mat4 TransformationSystem::get_relative_transform(const EntityID id) const
{
	return component(id).local_transform.get_mat4();
}

glm::vec3 TransformationSystem::get_relative_position(const EntityID id) const
{
	return component(id).local_transform.get_pos();
}

glm::vec3 TransformationSystem::get_relative_scale(const EntityID id) const
{
	return component(id).local_transform.get_scale();
}

glm::quat TransformationSystem::get_relative_rotation(const EntityID id) const
{
	return component(id).local_transform.get_orient();
}

void TransformationSystem::set_relative_transform(const EntityID id, const glm::mat4& value)
{
	component(id).local_transform.set_mat4(value);
	invalidate(id);
}

void TransformationSystem::set_relative_position(const EntityID id, const glm::vec3& value)
{
	component(id).local_transform.set_pos(value);
	invalidate(id);
}

void TransformationSystem::set_relative_scale(const EntityID id, const glm::vec3& value)
{
	component(id).local_transform.set_scale(value);
	invalidate(id);
}

void TransformationSystem::set_relative_rotation(const EntityID id, const glm::quat& value)
{
	component(id).local_transform.set_orient(value);
	invalidate(id);
}

bool TransformationSystem::attach_to(const EntityID child, const EntityID parent)
{
	auto& child_transform = component(child);
	component(parent);
	if (child == parent)
		return false;
	for (auto ancestor = std::optional<EntityID>(parent); ancestor;
		ancestor = component(*ancestor).parent)
		if (*ancestor == child)
			return false;
	if (child_transform.parent == parent)
		return true;

	const auto world = get_transform(child);
	if (child_transform.parent)
		component(*child_transform.parent).children.erase(child);
	child_transform.parent = parent;
	component(parent).children.insert(child);
	child_transform.local_transform.set_mat4(glm::inverse(get_transform(parent)) * world);
	child_transform.world_transform.set_mat4(world);
	child_transform.world_dirty = false;
	return true;
}

bool TransformationSystem::detach_from(const EntityID child)
{
	auto& child_transform = component(child);
	if (!child_transform.parent)
		return false;
	const auto world = get_transform(child);
	component(*child_transform.parent).children.erase(child);
	child_transform.parent.reset();
	child_transform.local_transform.set_mat4(world);
	child_transform.world_transform.set_mat4(world);
	child_transform.world_dirty = false;
	return true;
}

void TransformationSystem::detach_all_children(const EntityID parent)
{
	auto children = component(parent).children;
	for (const auto child : children)
		detach_from(child);
}

std::optional<EntityID> TransformationSystem::get_parent_id(const EntityID child) const
{
	return component(child).parent;
}

void TransformationSystem::remove_transformation(const EntityID id)
{
	if (!components.contains(id))
		return;
	detach_all_children(id);
	detach_from(id);
	components.erase(id);
}

void TransformationSystem::serialize(Serializer& out) const
{
	std::vector<EntityID> ids;
	for (const auto& [id, transform] : components)
		if (transform.persistence == TransformationPersistence::Persistent)
			ids.push_back(id);
	std::ranges::sort(ids);

	auto serialized = out.sequence("transformation_system");
	for (std::size_t index = 0; index < ids.size(); ++index)
	{
		const auto id = ids[index];
		const auto& transform = component(id);
		auto entry = serialized.append_map();
		entry.write("entity_id", id.get_underlying());
		if (transform.parent)
		{
			if (component(*transform.parent).persistence != TransformationPersistence::Persistent)
				throw SerializationError(
					"Persistent transformation has a transient parent at "
					+ transformation_path(index, "parent_id"));
			entry.write("parent_id", transform.parent->get_underlying());
		}
		else
			entry.write_null("parent_id");
		Serialization::write_transform(entry, "local_transform", transform.local_transform);
	}
}

void TransformationSystem::deserialize(const Deserializer& in)
{
	TransformationSystem restored;
	const auto entries = in.child("transformation_system").elements();
	for (std::size_t index = 0; index < entries.size(); ++index)
	{
		const auto& entry = entries[index];
		const EntityID id(entry.read<std::uint64_t>("entity_id"));
		if (restored.components.contains(id))
			throw SerializationError(
				"Duplicate transformation entity at "
				+ transformation_path(index, "entity_id"));
		restored.add_transformation(id);
		auto& transform = restored.component(id);
		transform.local_transform = Serialization::read_transform(entry, "local_transform");
		transform.world_transform = transform.local_transform;
		const auto parent = entry.child("parent_id");
		if (parent.kind() != SerializationKind::Null)
			transform.parent = EntityID(parent.as<std::uint64_t>());
	}

	for (std::size_t index = 0; index < entries.size(); ++index)
	{
		const EntityID id(entries[index].read<std::uint64_t>("entity_id"));
		auto& transform = restored.component(id);
		if (!transform.parent)
			continue;
		if (!restored.components.contains(*transform.parent))
			throw SerializationError(
				"Missing transformation parent at "
				+ transformation_path(index, "parent_id"));
		restored.component(*transform.parent).children.insert(id);
		transform.world_dirty = true;
	}

	for (std::size_t index = 0; index < entries.size(); ++index)
	{
		const EntityID id(entries[index].read<std::uint64_t>("entity_id"));
		std::unordered_set<EntityID> visited;
		for (auto current = std::optional<EntityID>(id); current;
			current = restored.component(*current).parent)
			if (!visited.insert(*current).second)
				throw SerializationError(
					"Cyclic transformation hierarchy at "
					+ transformation_path(index, "parent_id"));
	}

	auto transients = snapshot_transient_transformations();
	for (const auto& [id, transform] : restored.components)
	{
		if (transients.components.contains(id))
			throw SerializationError(
				"Persistent transformation conflicts with transient entity "
				+ std::to_string(id.get_underlying()));
		transients.add_transformation(id, transform.persistence);
		auto& destination = transients.component(id);
		destination.local_transform = transform.local_transform;
		destination.world_transform = transform.world_transform;
		destination.parent = transform.parent;
		destination.children = transform.children;
		destination.world_dirty = transform.world_dirty;
	}
	*this = std::move(transients);
}

TransformationSystem TransformationSystem::snapshot_transient_transformations() const
{
	TransformationSystem result;
	for (const auto& [id, transform] : components)
	{
		if (transform.persistence != TransformationPersistence::Transient)
			continue;
		result.add_transformation(id, TransformationPersistence::Transient);
		auto& copy = result.component(id);
		copy.local_transform = transform.local_transform;
		copy.world_transform = synced_world_transform(id);
		copy.parent = transform.parent;
		copy.world_dirty = transform.world_dirty;
	}
	for (auto& [id, transform] : result.components)
	{
		if (!transform.parent || !result.components.contains(*transform.parent))
		{
			transform.parent.reset();
			transform.local_transform = transform.world_transform;
			transform.world_dirty = false;
			continue;
		}
		result.component(*transform.parent).children.insert(id);
		transform.world_dirty = true;
	}
	return result;
}

TransformationSystem TransformationSystem::take_transient_transformations() const
{
	return snapshot_transient_transformations();
}
