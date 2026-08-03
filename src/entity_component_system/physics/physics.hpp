#pragma once

#include "entity_component_system/common.hpp"
#include "identifications.hpp"
#include "maths.hpp"

#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>

#include <memory>
#include <optional>
#include <span>
#include <variant>
#include <vector>

class ECS;
class Serializer;
class Deserializer;

enum class PhysicsMotionType { Static, Kinematic, Dynamic };
enum class PhysicsMotionQuality { Discrete, Continuous };
enum class PhysicsParticipation { Solid, Sensor, QueryOnly };
enum class PhysicsPersistence { Persistent, Transient };

struct BoxPhysicsShape { glm::vec3 half_extents{0.5f}; };
struct SpherePhysicsShape { float radius = 0.5f; };
struct CapsulePhysicsShape { float radius = 0.5f; float height = 2.0f; };
using PhysicsShape = std::variant<BoxPhysicsShape, SpherePhysicsShape, CapsulePhysicsShape>;

struct RigidBodyDefinition
{
	PhysicsShape shape = BoxPhysicsShape{};
	PhysicsMotionType motion = PhysicsMotionType::Static;
	PhysicsMotionQuality quality = PhysicsMotionQuality::Discrete;
	PhysicsParticipation participation = PhysicsParticipation::Solid;
	PhysicsPersistence persistence = PhysicsPersistence::Persistent;
	float mass = 1.0f;
	float friction = 0.5f;
	float restitution = 0.0f;
	float linear_damping = 0.05f;
	float angular_damping = 0.05f;
	float gravity_factor = 1.0f;
	bool enabled = true;
};

enum class PhysicsContactType { Begin, End };
struct PhysicsContactEvent { PhysicsContactType type; EntityID first; EntityID second; };

class PhysicsSystem
{
public:
	PhysicsSystem();
	~PhysicsSystem();
	PhysicsSystem(PhysicsSystem&&) noexcept;
	PhysicsSystem& operator=(PhysicsSystem&&) noexcept;
	PhysicsSystem(const PhysicsSystem&) = delete;
	PhysicsSystem& operator=(const PhysicsSystem&) = delete;

	void add_rigid_body(EntityID id, const RigidBodyDefinition& definition);
	void remove_rigid_body(EntityID id);
	bool has_rigid_body(EntityID id) const;
	void set_body_enabled(EntityID id, bool enabled);
	bool is_body_enabled(EntityID id) const;
	void teleport_body(EntityID id, glm::vec3 position, glm::quat rotation = glm::quat(1, 0, 0, 0), bool reset_velocity = true);
	void set_linear_velocity(EntityID id, glm::vec3 velocity);
	glm::vec3 get_linear_velocity(EntityID id) const;
	void add_impulse(EntityID id, glm::vec3 impulse);
	bool is_body_active(EntityID id) const;

	void set_gravity(glm::vec3 gravity);
	glm::vec3 get_gravity() const;
	void process(float delta_secs);

	DetectedEntityCollision raycast(const Maths::Ray& ray, std::optional<EntityID> ignored = std::nullopt) const;
	DetectedEntityCollision raycast(const Maths::Ray& ray, std::span<const EntityID> candidates) const;
	std::span<const PhysicsContactEvent> get_contact_events() const;

	void serialize(Serializer& out) const;
	void deserialize(const Deserializer& in);

protected:
	virtual ECS& get_ecs() = 0;
	virtual const ECS& get_ecs() const = 0;
	void remove_entity(EntityID id) { remove_rigid_body(id); }

private:
	class Impl;
	std::unique_ptr<Impl> impl;
};
