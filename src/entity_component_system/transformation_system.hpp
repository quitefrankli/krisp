#pragma once

#include "identifications.hpp"
#include "maths.hpp"

#include <glm/mat4x4.hpp>
#include <glm/gtc/quaternion.hpp>

#include <optional>
#include <unordered_map>
#include <unordered_set>


class Serializer;
class Deserializer;
class TransformationSystem;

enum class TransformationPersistence
{
	Persistent,
	Transient,
};

class TransformationComponent
{
	class ConstructionKey
	{
		friend class TransformationSystem;
		ConstructionKey() = default;
	};

public:
	TransformationComponent(const TransformationComponent&) = delete;
	TransformationComponent& operator=(const TransformationComponent&) = delete;
	TransformationComponent(TransformationComponent&&) = delete;
	TransformationComponent& operator=(TransformationComponent&&) = delete;
	TransformationComponent(
		const ConstructionKey&,
		TransformationSystem& owner,
		EntityID entity_id,
		TransformationPersistence persistence) :
		owner(&owner),
		entity_id(entity_id),
		persistence(persistence)
	{}

	EntityID get_entity_id() const { return entity_id; }

	Maths::Transform get_maths_transform() const;
	glm::mat4 get_transform() const;
	glm::vec3 get_position() const;
	glm::vec3 get_scale() const;
	glm::quat get_rotation() const;

	glm::mat4 get_relative_transform() const;
	glm::vec3 get_relative_position() const;
	glm::vec3 get_relative_scale() const;
	glm::quat get_relative_rotation() const;
	std::optional<EntityID> get_parent_id() const;

	void set_transform(const glm::mat4& transform);
	void set_position(const glm::vec3& position);
	void set_scale(float uniform_scale);
	void set_scale(const glm::vec3& scale);
	void set_rotation(const glm::quat& rotation);

	void set_relative_transform(const glm::mat4& transform);
	void set_relative_position(const glm::vec3& position);
	void set_relative_scale(const glm::vec3& scale);
	void set_relative_rotation(const glm::quat& rotation);

	bool attach_to(EntityID parent);
	bool attach_to(const TransformationComponent& parent);
	bool detach_from();
	void detach_all_children();

private:
	friend class TransformationSystem;

	TransformationSystem* owner;
	EntityID entity_id;
	Maths::Transform local_transform;
	mutable Maths::Transform world_transform;
	std::optional<EntityID> parent;
	std::unordered_set<EntityID> children;
	mutable bool world_dirty = false;
	TransformationPersistence persistence;
};

class TransformationSystem
{
public:
	TransformationSystem() = default;
	TransformationSystem(const TransformationSystem&) = delete;
	TransformationSystem& operator=(const TransformationSystem&) = delete;
	TransformationSystem(TransformationSystem&& other) noexcept;
	TransformationSystem& operator=(TransformationSystem&& other) noexcept;

	void add_transformation(
		EntityID id,
		TransformationPersistence persistence = TransformationPersistence::Persistent);
	void remove_transformation(EntityID id);
	bool has_transformation(EntityID id) const { return components.contains(id); }
	TransformationComponent& get_transformation(EntityID id);
	const TransformationComponent& get_transformation(EntityID id) const;

	Maths::Transform get_maths_transform(EntityID id) const;
	glm::mat4 get_transform(EntityID id) const;
	glm::vec3 get_position(EntityID id) const;
	glm::vec3 get_scale(EntityID id) const;
	glm::quat get_rotation(EntityID id) const;

	void set_transform(EntityID id, const glm::mat4& transform);
	void set_position(EntityID id, const glm::vec3& position);
	void set_scale(EntityID id, float uniform_scale) { set_scale(id, glm::vec3(uniform_scale)); }
	void set_scale(EntityID id, const glm::vec3& scale);
	void set_rotation(EntityID id, const glm::quat& rotation);

	glm::mat4 get_relative_transform(EntityID id) const;
	glm::vec3 get_relative_position(EntityID id) const;
	glm::vec3 get_relative_scale(EntityID id) const;
	glm::quat get_relative_rotation(EntityID id) const;

	void set_relative_transform(EntityID id, const glm::mat4& transform);
	void set_relative_position(EntityID id, const glm::vec3& position);
	void set_relative_scale(EntityID id, const glm::vec3& scale);
	void set_relative_rotation(EntityID id, const glm::quat& rotation);

	bool attach_to(EntityID child, EntityID parent);
	bool detach_from(EntityID child);
	void detach_all_children(EntityID parent);
	std::optional<EntityID> get_parent_id(EntityID child) const;

	void serialize(Serializer& out) const;
	void deserialize(const Deserializer& in);

	TransformationSystem take_transient_transformations() const;

private:
	TransformationComponent& component(EntityID id);
	const TransformationComponent& component(EntityID id) const;
	const Maths::Transform& synced_world_transform(EntityID id) const;
	void invalidate(EntityID id);
	void invalidate_children(EntityID id);
	void rebind_components();
	TransformationSystem snapshot_transient_transformations() const;

	std::unordered_map<EntityID, TransformationComponent> components;
};
