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

// Determines whether simulation can move a body.
enum class PhysicsMotionType { Static, Kinematic, Dynamic };
// Continuous motion uses swept collision detection to reduce tunnelling.
enum class PhysicsMotionQuality { Discrete, Continuous };
// Sensors report contacts without producing a collision response.
enum class PhysicsParticipation { Solid, Sensor, QueryOnly };
// Transient bodies are excluded from scene serialization.
enum class PhysicsPersistence { Persistent, Transient };

// Shape dimensions are expressed in local-space engine units.
struct BoxPhysicsShape { glm::vec3 half_extents{0.5f}; };
struct SpherePhysicsShape { float radius = 0.5f; };
struct CapsulePhysicsShape { float radius = 0.5f; float height = 2.0f; };
using PhysicsShape = std::variant<BoxPhysicsShape, SpherePhysicsShape, CapsulePhysicsShape>;

// Complete configuration used when creating a rigid body.
struct RigidBodyDefinition
{
	PhysicsShape shape = BoxPhysicsShape{};
	PhysicsMotionType motion = PhysicsMotionType::Static;
	PhysicsMotionQuality quality = PhysicsMotionQuality::Discrete;
	PhysicsParticipation participation = PhysicsParticipation::Solid;
	PhysicsPersistence persistence = PhysicsPersistence::Persistent;
	float mass = 1.0f;             // Used only by dynamic bodies.
	float friction = 0.5f;         // Tangential contact resistance.
	float restitution = 0.0f;      // Bounciness, normally in the range [0, 1].
	float linear_damping = 0.05f;  // Velocity decay applied over time.
	float angular_damping = 0.05f; // Angular velocity decay applied over time.
	float gravity_factor = 1.0f;   // Multiplier for the world's gravity.
	bool enabled = true;
};

// Contact events are published after each physics update and remain valid until
// the next call to process().
enum class PhysicsContactType { Begin, End };
struct PhysicsContactEvent { PhysicsContactType type; EntityID first; EntityID second; };
struct PhysicsDebugTriangle { glm::vec3 vertices[3]; };
struct PhysicsDebugBody
{
	EntityID entity;
	uint32_t body_id;
	glm::vec3 position;
	glm::quat rotation;
};

// Owns the Jolt simulation and synchronizes rigid bodies with ECS transforms.
class PhysicsSystem
{
public:
	PhysicsSystem();
	~PhysicsSystem();
	PhysicsSystem(PhysicsSystem&&) noexcept;
	PhysicsSystem& operator=(PhysicsSystem&&) noexcept;
	PhysicsSystem(const PhysicsSystem&) = delete;
	PhysicsSystem& operator=(const PhysicsSystem&) = delete;

	// Body transforms initially come from the corresponding ECS entity.
	void add_rigid_body(EntityID id, const RigidBodyDefinition& definition);
	void remove_rigid_body(EntityID id);
	bool has_rigid_body(EntityID id) const;
	// Disabled bodies remain allocated but do not participate in simulation.
	void set_body_enabled(EntityID id, bool enabled);
	bool is_body_enabled(EntityID id) const;
	// Teleports a body and optionally clears its linear and angular velocity.
	void teleport_body(EntityID id, glm::vec3 position, glm::quat rotation = glm::quat(1, 0, 0, 0), bool reset_velocity = true);
	void set_linear_velocity(EntityID id, glm::vec3 velocity);
	glm::vec3 get_linear_velocity(EntityID id) const;
	void set_angular_velocity(EntityID id, glm::vec3 velocity);
	glm::vec3 get_angular_velocity(EntityID id) const;
	void add_impulse(EntityID id, glm::vec3 impulse);
	bool is_body_active(EntityID id) const;
	// Overrides Jolt's combined restitution for contacts between this exact pair.
	// The override is removed automatically when either body is destroyed.
	void set_contact_restitution(EntityID first, EntityID second, float restitution);
	void clear_contact_restitution(EntityID first, EntityID second);

	void set_gravity(glm::vec3 gravity);
	glm::vec3 get_gravity() const;
	// Advances the fixed-step simulation and publishes dynamic transforms/events.
	void process(float delta_secs);

	// Returns the closest hit, or a result with bCollided == false.
	DetectedEntityCollision raycast(const Maths::Ray& ray, std::optional<EntityID> ignored = std::nullopt) const;
	DetectedEntityCollision raycast(const Maths::Ray& ray, std::span<const EntityID> candidates) const;
	std::span<const PhysicsContactEvent> get_contact_events() const;
	std::vector<PhysicsDebugBody> get_debug_bodies() const;
	std::vector<PhysicsDebugTriangle> get_debug_shape_triangles(EntityID id) const;

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
