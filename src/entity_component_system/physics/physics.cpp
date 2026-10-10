#include "physics.hpp"

#include "entity_component_system/ecs.hpp"
#include "serialization/serialization_helpers.hpp"
#include "constants.hpp"

// Conan supplies Jolt as a release library even when Krisp itself is a debug
// build. Keep Jolt's header ABI aligned with that library.
#ifdef _DEBUG
#undef _DEBUG
#endif
#define JPH_NO_DEBUG
#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <unordered_map>

using JPH::Body; using JPH::BodyCreationSettings; using JPH::BodyID; using JPH::BodyLockRead;
using JPH::BroadPhaseLayer; using JPH::BroadPhaseLayerInterface; using JPH::ContactListener;
using JPH::ContactManifold; using JPH::ContactSettings; using JPH::EActivation; using JPH::EMotionQuality;
using JPH::EMotionType; using JPH::EOverrideMassProperties; using JPH::Factory; using JPH::JobSystem;
using JPH::JobSystemSingleThreaded; using JPH::JobSystemThreadPool;
using JPH::ObjectLayer; using JPH::ObjectLayerPairFilter; using JPH::ObjectVsBroadPhaseLayerFilter;
using JPH::Quat; using JPH::RayCastResult; using JPH::RegisterDefaultAllocator; using JPH::RegisterTypes;
using JPH::RayCast; using JPH::RRayCast; using JPH::RVec3; using JPH::ShapeRefC; using JPH::SubShapeIDCreator;
using JPH::SubShapeIDPair; using JPH::TempAllocatorImpl; using JPH::UnregisterTypes; using JPH::Vec3;
using JPH::Vec3Arg; using JPH::BoxShape; using JPH::CapsuleShape; using JPH::SphereShape;
using JPH::CharacterVirtual; using JPH::CharacterVirtualSettings;
using JPH::cMaxPhysicsBarriers; using JPH::cMaxPhysicsJobs;

namespace {
constexpr ObjectLayer STATIC_LAYER = 0;
constexpr ObjectLayer MOVING_LAYER = 1;
constexpr ObjectLayer SENSOR_LAYER = 2;
constexpr uint NUM_LAYERS = 3;
constexpr BroadPhaseLayer BP_STATIC(0), BP_MOVING(1), BP_SENSOR(2);

std::string physics_entry_path(const Deserializer& entry, std::string_view field)
{
	return entry.path() + "." + std::string(field);
}

void require_finite(float value, const std::string& path)
{
	if (!std::isfinite(value)) throw SerializationError("Non-finite number at " + path);
}

void require_finite(glm::vec3 value, const std::string& path)
{
	if (!std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.z))
		throw SerializationError("Non-finite vector at " + path);
}

template<typename E>
E read_enum(const Deserializer& in, const char* field, int max, const std::string& path)
{
	const int value = in.read<int>(field);
	if (value < 0 || value > max) throw SerializationError("Invalid enum at " + path);
	return static_cast<E>(value);
}

Vec3 to_jolt(glm::vec3 v) { return {v.x, v.y, v.z}; }
RVec3 to_jolt_r(glm::vec3 v) { return {v.x, v.y, v.z}; }
Quat to_jolt(glm::quat q) { return {q.x, q.y, q.z, q.w}; }
glm::vec3 to_glm(Vec3Arg v) { return {v.GetX(), v.GetY(), v.GetZ()}; }
class Layers final : public BroadPhaseLayerInterface, public ObjectVsBroadPhaseLayerFilter, public ObjectLayerPairFilter
{
public:
	uint GetNumBroadPhaseLayers() const override { return 3; }
	BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer l) const override
	{
		return l == STATIC_LAYER ? BP_STATIC : l == SENSOR_LAYER ? BP_SENSOR : BP_MOVING;
	}
	bool ShouldCollide(ObjectLayer l, BroadPhaseLayer b) const override
	{
		if (l == STATIC_LAYER) return b == BP_MOVING;
		if (l == SENSOR_LAYER) return b == BP_MOVING;
		return b == BP_STATIC || b == BP_MOVING || b == BP_SENSOR;
	}
	bool ShouldCollide(ObjectLayer a, ObjectLayer b) const override
	{
		if (a == STATIC_LAYER) return b == MOVING_LAYER;
		if (a == SENSOR_LAYER) return b == MOVING_LAYER;
		return b == STATIC_LAYER || b == MOVING_LAYER || b == SENSOR_LAYER;
	}
};

class CharacterBroadPhaseFilter final : public JPH::BroadPhaseLayerFilter
{
public:
	bool ShouldCollide(BroadPhaseLayer layer) const override { return layer != BP_SENSOR; }
};

class CharacterObjectLayerFilter final : public JPH::ObjectLayerFilter
{
public:
	explicit CharacterObjectLayerFilter(const Layers& layers) : layers(layers) {}
	bool ShouldCollide(ObjectLayer layer) const override
	{
		return layer != SENSOR_LAYER && layers.ShouldCollide(MOVING_LAYER, layer);
	}
private:
	const Layers& layers;
};

struct Runtime
{
	Runtime()
	{
		RegisterDefaultAllocator();
		Factory::sInstance = new Factory;
		RegisterTypes();
	}
	~Runtime()
	{
		UnregisterTypes();
		delete Factory::sInstance;
		Factory::sInstance = nullptr;
	}
};
Runtime& runtime() { static Runtime value; return value; }

ObjectLayer layer_for(const RigidBodyDefinition& d)
{
	if (d.participation == PhysicsParticipation::Sensor) return SENSOR_LAYER;
	return d.motion == PhysicsMotionType::Static ? STATIC_LAYER : MOVING_LAYER;
}
EMotionType motion_for(PhysicsMotionType m)
{
	return m == PhysicsMotionType::Static ? EMotionType::Static : m == PhysicsMotionType::Kinematic ? EMotionType::Kinematic : EMotionType::Dynamic;
}

std::unique_ptr<JobSystem> make_job_system()
{
	if constexpr (CSTS::PHYSICS_WORKER_THREADS == 0)
		return std::make_unique<JobSystemSingleThreaded>(cMaxPhysicsJobs);
	return std::make_unique<JobSystemThreadPool>(
		cMaxPhysicsJobs, cMaxPhysicsBarriers, CSTS::PHYSICS_WORKER_THREADS);
}

void validate_controller_definition(const CharacterControllerDefinition& definition)
{
	for (const float value : {definition.radius, definition.height, definition.max_slope_angle,
		definition.step_height, definition.ground_snap_distance})
		if (!std::isfinite(value)) throw std::invalid_argument("Character controller dimensions must be finite");
	if (definition.radius <= 0.0f || definition.height < 2.0f * definition.radius)
		throw std::invalid_argument("Character controller capsule dimensions are invalid");
	if (definition.max_slope_angle < 0.0f || definition.max_slope_angle >= 1.57079632679f)
		throw std::invalid_argument("Character controller slope angle must be in [0, pi/2)");
	if (definition.step_height < 0.0f || definition.ground_snap_distance < 0.0f)
		throw std::invalid_argument("Character controller step and snap distances cannot be negative");
}
}

class ::PhysicsSystem::Impl final : public ContactListener
{
public:
	struct BodyRecord
	{
		BodyID body;
		RigidBodyDefinition definition;
		glm::vec3 published_position;
		glm::quat published_rotation;
	};
	struct ControllerRecord
	{
		JPH::Ref<CharacterVirtual> character;
		CharacterControllerDefinition definition;
		glm::vec3 desired_velocity{0.0f};
		glm::vec3 published_position{0.0f};
		bool launch_locked = false;
	};
	class ControllerBodyFilter final : public JPH::BodyFilter
	{
	public:
		ControllerBodyFilter(BodyID inner_body, const std::unordered_map<EntityID, BodyRecord>& rigid_bodies)
			: inner_body(inner_body), rigid_bodies(rigid_bodies) {}
		bool ShouldCollide(const BodyID& body_id) const override { return body_id != inner_body; }
		bool ShouldCollideLocked(const Body& body) const override
		{
			if (body.IsSensor()) return false;
			const auto found = rigid_bodies.find(EntityID(body.GetUserData()));
			return found == rigid_bodies.end() || found->second.definition.participation != PhysicsParticipation::QueryOnly;
		}
	private:
		BodyID inner_body;
		const std::unordered_map<EntityID, BodyRecord>& rigid_bodies;
	};
	struct EntityPair
	{
		EntityID first;
		EntityID second;
		auto operator<=>(const EntityPair&) const = default;
	};
	struct EntityPairHash
	{
		std::size_t operator()(const EntityPair& pair) const
		{
			const auto first = std::hash<EntityID>{}(pair.first);
			const auto second = std::hash<EntityID>{}(pair.second);
			return first ^ (second + 0x9e3779b9 + (first << 6) + (first >> 2));
		}
	};
	static EntityPair ordered_pair(EntityID first, EntityID second)
	{
		return first < second ? EntityPair{first, second} : EntityPair{second, first};
	}

	Impl() : runtime_ref(runtime()), allocator(10 * 1024 * 1024), jobs(make_job_system())
	{
		world.Init(65536, 0, 65536, 10240, layers, layers, layers);
		world.SetContactListener(this);
	}
	~Impl() override
	{
		controllers.clear();
		auto& bodies_api = world.GetBodyInterface();
		for (const auto& [_, r] : bodies) {
			if (bodies_api.IsAdded(r.body)) bodies_api.RemoveBody(r.body);
			bodies_api.DestroyBody(r.body);
		}
	}

	void add_controller(EntityID id, const CharacterControllerDefinition& definition, glm::vec3 position)
	{
		validate_controller_definition(definition);
		if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z))
			throw std::invalid_argument("Character controller position must be finite");
		const float half_height = definition.height * 0.5f;
		ShapeRefC capsule = new CapsuleShape(half_height - definition.radius, definition.radius);
		CharacterVirtualSettings settings;
		settings.mShape = capsule;
		settings.mShapeOffset = Vec3(0.0f, half_height, 0.0f);
		settings.mMaxSlopeAngle = definition.max_slope_angle;
		settings.mInnerBodyShape = capsule;
		settings.mInnerBodyLayer = MOVING_LAYER;
		JPH::Ref<CharacterVirtual> character = new CharacterVirtual(&settings, to_jolt_r(position), Quat::sIdentity(), id.get_underlying(), &world);
		if (character->GetInnerBodyID().IsInvalid()) throw std::runtime_error("Jolt could not create character controller proxy body");
		controllers.emplace(id, ControllerRecord{std::move(character), definition, glm::vec3(0.0f), position, false});
		const CharacterBroadPhaseFilter broad_phase;
		const CharacterObjectLayerFilter object_layer(layers);
		const ControllerBodyFilter body_filter(controllers.at(id).character->GetInnerBodyID(), bodies);
		const JPH::ShapeFilter shape_filter;
		controllers.at(id).character->RefreshContacts(broad_phase, object_layer, body_filter, shape_filter, allocator);
	}

	glm::vec3 update_controller(EntityID id, ControllerRecord& record, float delta_time, glm::vec3 transform_position)
	{
		auto& character = *record.character;
		if (glm::distance(transform_position, record.published_position) > 0.0001f) {
			character.SetPosition(to_jolt_r(transform_position));
			character.SetLinearVelocity(Vec3::sZero());
			record.launch_locked = false;
		}
		Vec3 velocity = character.GetLinearVelocity();
		const Vec3 up = Vec3::sAxisY();
		const Vec3 gravity = world.GetGravity();
		const bool grounded = character.GetGroundState() == CharacterVirtual::EGroundState::OnGround && velocity.Dot(up) <= 0.0f;
		if (grounded) {
			velocity = character.GetGroundVelocity();
			record.launch_locked = false;
		}
		velocity.SetX((grounded ? velocity.GetX() : 0.0f) + record.desired_velocity.x);
		velocity.SetZ((grounded ? velocity.GetZ() : 0.0f) + record.desired_velocity.z);
		velocity += up * gravity.Dot(up) * delta_time;
		character.SetLinearVelocity(velocity);

		const CharacterBroadPhaseFilter broad_phase;
		const CharacterObjectLayerFilter object_layer(layers);
		const ControllerBodyFilter body_filter(character.GetInnerBodyID(), bodies);
		const JPH::ShapeFilter shape_filter;
		CharacterVirtual::ExtendedUpdateSettings update_settings;
		update_settings.mWalkStairsStepUp = Vec3(0.0f, record.definition.step_height, 0.0f);
		update_settings.mStickToFloorStepDown = character.GetLinearVelocity().Dot(up) <= 0.0f
			? Vec3(0.0f, -record.definition.ground_snap_distance, 0.0f) : Vec3::sZero();
		character.ExtendedUpdate(delta_time, gravity, update_settings, broad_phase, object_layer,
			body_filter, shape_filter, allocator);
		if (character.GetGroundState() == CharacterVirtual::EGroundState::OnGround) {
			Vec3 actual_velocity = character.GetLinearVelocity();
			const Vec3 support_velocity = character.GetGroundVelocity();
			if ((actual_velocity - support_velocity).Dot(up) <= 0.0f) {
				actual_velocity.SetY(support_velocity.GetY());
				character.SetLinearVelocity(actual_velocity);
				record.launch_locked = false;
			}
		}
		const RVec3 world_position = character.GetPosition();
		record.published_position = to_glm(Vec3(world_position));
		return record.published_position;
	}

	BodyID query_body(EntityID id) const
	{
		if (const auto it = bodies.find(id); it != bodies.end()) return it->second.body;
		if (const auto it = controllers.find(id); it != controllers.end()) return it->second.character->GetInnerBodyID();
		return BodyID();
	}

	ShapeRefC shape(const RigidBodyDefinition& d)
	{
		ShapeRefC result = std::visit([&](const auto& s) -> ShapeRefC {
			using T = std::decay_t<decltype(s)>;
			if constexpr (std::is_same_v<T, BoxPhysicsShape>) return new BoxShape(to_jolt(s.half_extents));
			else if constexpr (std::is_same_v<T, SpherePhysicsShape>) return new SphereShape(s.radius);
			else if constexpr (std::is_same_v<T, CapsulePhysicsShape>) return new CapsuleShape(std::max(0.0f, s.height * 0.5f - s.radius), s.radius);
			else {
				if (d.motion != PhysicsMotionType::Static)
					throw std::invalid_argument("Triangle mesh physics shapes must be static");
				if (s.vertices.empty() || s.indices.empty() || s.indices.size() % 3 != 0)
					throw std::invalid_argument("Triangle mesh requires vertices and complete indexed triangles");
				if (s.vertices.size() > std::numeric_limits<JPH::uint32>::max())
					throw std::invalid_argument("Triangle mesh has too many vertices");

				JPH::VertexList vertices;
				vertices.reserve(s.vertices.size());
				for (const glm::vec3 vertex : s.vertices) {
					if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y) || !std::isfinite(vertex.z))
						throw std::invalid_argument("Triangle mesh vertices must be finite");
					vertices.emplace_back(vertex.x, vertex.y, vertex.z);
				}
				JPH::IndexedTriangleList triangles;
				triangles.reserve(s.indices.size() / 3);
				for (std::size_t offset = 0; offset < s.indices.size(); offset += 3) {
					const uint32_t a = s.indices[offset], b = s.indices[offset + 1], c = s.indices[offset + 2];
					if (a >= s.vertices.size() || b >= s.vertices.size() || c >= s.vertices.size())
						throw std::invalid_argument("Triangle mesh index is outside the vertex array");
					JPH::IndexedTriangle triangle(a, b, c, 0);
					if (triangle.IsDegenerate(vertices))
						throw std::invalid_argument("Triangle mesh contains a degenerate triangle");
					triangles.push_back(triangle);
				}
				JPH::MeshShapeSettings settings(std::move(vertices), std::move(triangles));
				auto created = settings.Create();
				if (created.HasError())
					throw std::runtime_error("Jolt could not create triangle mesh shape: " + std::string(created.GetError().c_str()));
				return created.Get();
			}
		}, d.shape);
		if (d.shape_offset != glm::vec3(0.0f))
			return new JPH::RotatedTranslatedShape(to_jolt(d.shape_offset), Quat::sIdentity(), result.GetPtr());
		return result;
	}

	BodyID add_body(EntityID id, const RigidBodyDefinition& d, ShapeRefC body_shape, glm::vec3 position, glm::quat rotation)
	{
		BodyCreationSettings settings(body_shape, to_jolt_r(position), to_jolt(rotation), motion_for(d.motion), layer_for(d));
		settings.mUserData = id.get_underlying();
		settings.mFriction = d.friction;
		settings.mRestitution = d.restitution;
		settings.mLinearDamping = d.linear_damping;
		settings.mAngularDamping = d.angular_damping;
		settings.mGravityFactor = d.gravity_factor;
		settings.mMotionQuality = d.quality == PhysicsMotionQuality::Continuous ? EMotionQuality::LinearCast : EMotionQuality::Discrete;
		settings.mIsSensor = d.participation == PhysicsParticipation::Sensor;
		if (d.motion == PhysicsMotionType::Dynamic) settings.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia, settings.mMassPropertiesOverride.mMass = d.mass;
		Body* body = world.GetBodyInterface().CreateBody(settings);
		if (!body) throw std::runtime_error("Jolt body capacity exhausted");
		bodies.emplace(id, BodyRecord{body->GetID(), d, position, rotation});
		if (d.enabled) world.GetBodyInterface().AddBody(body->GetID(), d.motion == PhysicsMotionType::Dynamic ? EActivation::Activate : EActivation::DontActivate);
		return body->GetID();
	}

	void OnContactAdded(const Body& a, const Body& b, const ContactManifold&, ContactSettings& settings) override
	{
		const auto override = restitution_overrides.find(ordered_pair(EntityID(a.GetUserData()), EntityID(b.GetUserData())));
		if (override != restitution_overrides.end()) settings.mCombinedRestitution = override->second;
		add_event(PhysicsContactType::Begin, a, b);
	}
	void OnContactRemoved(const SubShapeIDPair& pair) override
	{
		const auto& lock = world.GetBodyLockInterfaceNoLock();
		BodyLockRead a(lock, pair.GetBody1ID()), b(lock, pair.GetBody2ID());
		if (a.Succeeded() && b.Succeeded()) add_event(PhysicsContactType::End, a.GetBody(), b.GetBody());
	}
	void add_event(PhysicsContactType type, const Body& a, const Body& b)
	{
		std::scoped_lock lock(event_mutex);
		pending_events.push_back({type, EntityID(a.GetUserData()), EntityID(b.GetUserData())});
	}

	Runtime& runtime_ref;
	Layers layers;
	TempAllocatorImpl allocator;
	std::unique_ptr<JobSystem> jobs;
	JPH::PhysicsSystem world;
	std::unordered_map<EntityID, BodyRecord> bodies;
	std::unordered_map<EntityID, ControllerRecord> controllers;
	std::unordered_map<EntityPair, float, EntityPairHash> restitution_overrides;
	std::vector<PhysicsContactEvent> events, pending_events;
	std::mutex event_mutex;
	float accumulator = 0.0f;
};

::PhysicsSystem::PhysicsSystem() : impl(std::make_unique<Impl>()) {}
::PhysicsSystem::~PhysicsSystem() = default;
::PhysicsSystem::PhysicsSystem(PhysicsSystem&&) noexcept = default;
::PhysicsSystem& ::PhysicsSystem::operator=(::PhysicsSystem&&) noexcept = default;

void ::PhysicsSystem::add_rigid_body(EntityID id, const RigidBodyDefinition& d)
{
	require_finite(d.shape_offset, "shape_offset");
	if (has_character_controller(id)) throw std::invalid_argument("Entity already has a character controller");
	if (!get_ecs().has_transformation(id)) throw std::invalid_argument("Rigid body entity has no transformation");
	const ShapeRefC body_shape = impl->shape(d);
	remove_rigid_body(id);
	const auto position = get_ecs().get_position(id);
	const auto rotation = get_ecs().get_rotation(id);
	impl->add_body(id, d, body_shape, position, rotation);
}

void ::PhysicsSystem::remove_rigid_body(EntityID id)
{
	auto it = impl->bodies.find(id);
	if (it == impl->bodies.end()) return;
	auto& api = impl->world.GetBodyInterface();
	if (api.IsAdded(it->second.body)) api.RemoveBody(it->second.body);
	api.DestroyBody(it->second.body);
	impl->bodies.erase(it);
	std::erase_if(impl->restitution_overrides, [id](const auto& entry) {
		return entry.first.first == id || entry.first.second == id;
	});
}
bool ::PhysicsSystem::has_rigid_body(EntityID id) const { return impl->bodies.contains(id); }

void ::PhysicsSystem::add_character_controller(EntityID id, const CharacterControllerDefinition& definition)
{
	if (has_rigid_body(id)) throw std::invalid_argument("Entity already has a rigid body");
	if (!get_ecs().has_transformation(id)) throw std::invalid_argument("Character controller entity has no transformation");
	validate_controller_definition(definition);
	remove_character_controller(id);
	impl->add_controller(id, definition, get_ecs().get_position(id));
}

void ::PhysicsSystem::remove_character_controller(EntityID id)
{
	impl->controllers.erase(id);
	std::erase_if(impl->restitution_overrides, [id](const auto& entry) {
		return entry.first.first == id || entry.first.second == id;
	});
}

bool ::PhysicsSystem::has_character_controller(EntityID id) const { return impl->controllers.contains(id); }

void ::PhysicsSystem::set_character_desired_velocity(EntityID id, glm::vec3 velocity)
{
	require_finite(velocity, "character desired velocity");
	velocity.y = 0.0f;
	impl->controllers.at(id).desired_velocity = velocity;
}

glm::vec3 PhysicsSystem::get_character_velocity(EntityID id) const
{
	return to_glm(impl->controllers.at(id).character->GetLinearVelocity());
}

bool ::PhysicsSystem::is_character_grounded(EntityID id) const
{
	const auto& record = impl->controllers.at(id);
	return !record.launch_locked
		&& record.character->GetGroundState() == CharacterVirtual::EGroundState::OnGround;
}

bool ::PhysicsSystem::launch_character(EntityID id, float upward_speed)
{
	if (!std::isfinite(upward_speed) || upward_speed <= 0.0f)
		throw std::invalid_argument("Character launch speed must be finite and positive");
	auto& record = impl->controllers.at(id);
	if (record.launch_locked || record.character->GetGroundState() != CharacterVirtual::EGroundState::OnGround)
		return false;
	Vec3 velocity = record.character->GetLinearVelocity();
	velocity.SetY(upward_speed);
	record.character->SetLinearVelocity(velocity);
	record.launch_locked = true;
	return true;
}

void ::PhysicsSystem::teleport_character_controller(EntityID id, glm::vec3 position)
{
	require_finite(position, "character position");
	auto& record = impl->controllers.at(id);
	record.character->SetPosition(to_jolt_r(position));
	record.character->SetLinearVelocity(Vec3::sZero());
	record.desired_velocity = glm::vec3(0.0f);
	record.launch_locked = false;
	record.published_position = position;
	get_ecs().set_position(id, position);
	const CharacterBroadPhaseFilter broad_phase;
	const CharacterObjectLayerFilter object_layer(impl->layers);
	const Impl::ControllerBodyFilter body_filter(record.character->GetInnerBodyID(), impl->bodies);
	const JPH::ShapeFilter shape_filter;
	record.character->RefreshContacts(broad_phase, object_layer, body_filter, shape_filter, impl->allocator);
}

void ::PhysicsSystem::set_body_enabled(EntityID id, bool enabled)
{
	auto& r = impl->bodies.at(id); auto& api = impl->world.GetBodyInterface();
	if (enabled && !api.IsAdded(r.body)) api.AddBody(r.body, EActivation::Activate);
	else if (!enabled && api.IsAdded(r.body)) api.RemoveBody(r.body);
}
bool ::PhysicsSystem::is_body_enabled(EntityID id) const { return impl->world.GetBodyInterface().IsAdded(impl->bodies.at(id).body); }
void ::PhysicsSystem::teleport_body(EntityID id, glm::vec3 p, glm::quat q, bool reset)
{
	auto& r = impl->bodies.at(id); auto& api = impl->world.GetBodyInterface();
	api.SetPositionAndRotation(r.body, to_jolt_r(p), to_jolt(q), EActivation::Activate);
	if (reset) { api.SetLinearVelocity(r.body, Vec3::sZero()); api.SetAngularVelocity(r.body, Vec3::sZero()); }
	r.published_position = p; r.published_rotation = q; get_ecs().set_position(id, p); get_ecs().set_rotation(id, q);
}
void ::PhysicsSystem::set_linear_velocity(EntityID id, glm::vec3 v) { impl->world.GetBodyInterface().SetLinearVelocity(impl->bodies.at(id).body, to_jolt(v)); }
glm::vec3 PhysicsSystem::get_linear_velocity(EntityID id) const { return to_glm(impl->world.GetBodyInterface().GetLinearVelocity(impl->bodies.at(id).body)); }
void ::PhysicsSystem::set_angular_velocity(EntityID id, glm::vec3 v) { impl->world.GetBodyInterface().SetAngularVelocity(impl->bodies.at(id).body, to_jolt(v)); }
glm::vec3 PhysicsSystem::get_angular_velocity(EntityID id) const { return to_glm(impl->world.GetBodyInterface().GetAngularVelocity(impl->bodies.at(id).body)); }
void ::PhysicsSystem::add_impulse(EntityID id, glm::vec3 v) { impl->world.GetBodyInterface().AddImpulse(impl->bodies.at(id).body, to_jolt(v)); }
bool ::PhysicsSystem::is_body_active(EntityID id) const { return impl->world.GetBodyInterface().IsActive(impl->bodies.at(id).body); }
void ::PhysicsSystem::set_body_active(EntityID id, bool active)
{
	auto& api = impl->world.GetBodyInterface();
	const auto body = impl->bodies.at(id).body;
	if (!api.IsAdded(body)) throw std::invalid_argument("Cannot activate a disabled rigid body");
	if (active) api.ActivateBody(body);
	else api.DeactivateBody(body);
}
void ::PhysicsSystem::set_contact_restitution(EntityID first, EntityID second, float restitution)
{
	if (restitution < 0.0f) throw std::invalid_argument("Contact restitution cannot be negative");
	impl->restitution_overrides.insert_or_assign(Impl::ordered_pair(first, second), restitution);
}
void ::PhysicsSystem::clear_contact_restitution(EntityID first, EntityID second)
{
	impl->restitution_overrides.erase(Impl::ordered_pair(first, second));
}
void ::PhysicsSystem::set_gravity(glm::vec3 g) { impl->world.SetGravity(to_jolt(g)); }
glm::vec3 PhysicsSystem::get_gravity() const { return to_glm(impl->world.GetGravity()); }

void ::PhysicsSystem::process(float dt)
{
	auto& api = impl->world.GetBodyInterface();
	for (auto& [id, r] : impl->bodies) {
		if (!api.IsAdded(r.body)) continue;
		const auto p = get_ecs().get_position(id); const auto q = get_ecs().get_rotation(id);
		if (glm::distance(p, r.published_position) > 0.0001f || std::abs(glm::dot(q, r.published_rotation)) < 0.99999f)
			api.SetPositionAndRotation(r.body, to_jolt_r(p), to_jolt(q), EActivation::Activate);
	}
	impl->accumulator = std::min(impl->accumulator + std::max(0.0f, dt), 4.0f / 60.0f);
	while (impl->accumulator >= 1.0f / 60.0f) {
		for (auto& [id, controller] : impl->controllers) {
			const auto position = impl->update_controller(id, controller, 1.0f / 60.0f, get_ecs().get_position(id));
			get_ecs().set_position(id, position);
		}
		impl->world.Update(1.0f / 60.0f, 1, &impl->allocator, impl->jobs.get());
		impl->accumulator -= 1.0f / 60.0f;
	}
	for (auto& [id, r] : impl->bodies) if (r.definition.motion == PhysicsMotionType::Dynamic && api.IsAdded(r.body)) {
		r.published_position = to_glm(Vec3(api.GetPosition(r.body))); r.published_rotation = {api.GetRotation(r.body).GetW(), api.GetRotation(r.body).GetX(), api.GetRotation(r.body).GetY(), api.GetRotation(r.body).GetZ()};
		get_ecs().set_position(id, r.published_position); get_ecs().set_rotation(id, r.published_rotation);
	}
	std::scoped_lock lock(impl->event_mutex); impl->events = std::move(impl->pending_events); impl->pending_events.clear();
}

DetectedEntityCollision PhysicsSystem::raycast(const Maths::Ray& ray, std::optional<EntityID> ignored) const
{
	RayCastResult hit; RRayCast cast(to_jolt_r(ray.origin), to_jolt(ray.direction * 10000.0f));
	if (!impl->world.GetNarrowPhaseQuery().CastRay(cast, hit)) return {};
	const EntityID id(impl->world.GetBodyInterface().GetUserData(hit.mBodyID));
	if (ignored && id == *ignored) {
		std::vector<EntityID> candidates;
		for (const auto& [candidate, _] : impl->bodies) if (candidate != id) candidates.push_back(candidate);
		for (const auto& [candidate, _] : impl->controllers) if (candidate != id) candidates.push_back(candidate);
		return raycast(ray, candidates);
	}
	return {true, id, ray.origin + ray.direction * (10000.0f * hit.mFraction)};
}
DetectedEntityCollision PhysicsSystem::raycast(const Maths::Ray& ray, std::span<const EntityID> candidates) const
{
	DetectedEntityCollision best; float distance = 10000.0f;
	for (EntityID id : candidates) {
		const BodyID body_id = impl->query_body(id);
		if (body_id.IsInvalid()) continue;
		RayCastResult hit;
		BodyLockRead lock(impl->world.GetBodyLockInterface(), body_id);
		if (!lock.Succeeded()) continue;
		const Body& body = lock.GetBody();
		const Quat inverse = body.GetRotation().Conjugated();
		const RayCast cast(inverse * (to_jolt(ray.origin) - Vec3(body.GetCenterOfMassPosition())), inverse * to_jolt(ray.direction * distance));
		if (body.GetShape()->CastRay(cast, SubShapeIDCreator{}, hit)) {
			distance *= hit.mFraction; best = {true, id, ray.origin + ray.direction * distance};
		}
	}
	return best;
}
std::span<const PhysicsContactEvent> PhysicsSystem::get_contact_events() const { return impl->events; }
std::vector<PhysicsDebugBody> PhysicsSystem::get_debug_bodies() const
{
	std::vector<PhysicsDebugBody> bodies;
	bodies.reserve(impl->bodies.size() + impl->controllers.size());
	for (const auto& [entity, record] : impl->bodies) {
		BodyLockRead lock(impl->world.GetBodyLockInterface(), record.body);
		if (!lock.Succeeded() || !lock.GetBody().IsInBroadPhase()) continue;
		const Body& body = lock.GetBody();
		const Quat rotation = body.GetRotation();
		bodies.push_back({entity, record.body.GetIndexAndSequenceNumber(),
			to_glm(Vec3(body.GetCenterOfMassPosition())),
			{rotation.GetW(), rotation.GetX(), rotation.GetY(), rotation.GetZ()}});
	}
	for (const auto& [entity, record] : impl->controllers) {
		const BodyID body_id = record.character->GetInnerBodyID();
		BodyLockRead lock(impl->world.GetBodyLockInterface(), body_id);
		if (!lock.Succeeded() || !lock.GetBody().IsInBroadPhase()) continue;
		const Body& body = lock.GetBody();
		const Quat rotation = body.GetRotation();
		bodies.push_back({entity, body_id.GetIndexAndSequenceNumber(),
			to_glm(Vec3(body.GetCenterOfMassPosition())),
			{rotation.GetW(), rotation.GetX(), rotation.GetY(), rotation.GetZ()}});
	}
	return bodies;
}

std::vector<PhysicsDebugTriangle> PhysicsSystem::get_debug_shape_triangles(EntityID id) const
{
	constexpr int batch_size = 256;
	std::vector<PhysicsDebugTriangle> triangles;
	JPH::Float3 vertices[batch_size * 3];
	const BodyID body_id = impl->query_body(id);
	if (body_id.IsInvalid()) return triangles;
	BodyLockRead lock(impl->world.GetBodyLockInterface(), body_id);
	if (!lock.Succeeded()) return triangles;
	const auto* shape = lock.GetBody().GetShape();
	// Translation-only wrappers shift the body's centre of mass. Debug geometry
	// remains centre-of-mass-relative and must be extracted from the leaf shape.
	if (shape->GetSubType() == JPH::EShapeSubType::RotatedTranslated)
		shape = static_cast<const JPH::RotatedTranslatedShape*>(shape)->GetInnerShape();
	JPH::Shape::GetTrianglesContext context;
	shape->GetTrianglesStart(context, shape->GetLocalBounds(), Vec3::sZero(), Quat::sIdentity(),
		Vec3::sReplicate(1.0f));
	for (int count; (count = shape->GetTrianglesNext(context, batch_size, vertices)) > 0; ) {
		triangles.reserve(triangles.size() + count);
		for (int triangle = 0; triangle < count; ++triangle) {
			const auto* v = vertices + triangle * 3;
			triangles.push_back({{{v[0].x, v[0].y, v[0].z},
				{v[1].x, v[1].y, v[1].z}, {v[2].x, v[2].y, v[2].z}}});
		}
	}
	return triangles;
}

void ::PhysicsSystem::serialize(Serializer& out) const
{
	auto system = out.map("physics_system");
	Serialization::write_vec3(system, "gravity", get_gravity());
	auto bodies = system.sequence("bodies");
	const auto& api = impl->world.GetBodyInterface();
	for (const auto& [id, record] : impl->bodies) {
		if (record.definition.persistence == PhysicsPersistence::Transient
			|| get_ecs().is_transient_transformation(id)) continue;
		auto body = bodies.append_map();
		body.write("entity_id", id.get_underlying());
		body.write("shape", static_cast<int>(record.definition.shape.index()));
		std::visit([&](const auto& shape) {
			using T = std::decay_t<decltype(shape)>;
			if constexpr (std::is_same_v<T, BoxPhysicsShape>) Serialization::write_vec3(body, "half_extents", shape.half_extents);
			else if constexpr (std::is_same_v<T, SpherePhysicsShape>) body.write("radius", shape.radius);
			else if constexpr (std::is_same_v<T, CapsulePhysicsShape>) { body.write("radius", shape.radius); body.write("height", shape.height); }
			else {
				auto vertices = body.sequence("vertices");
				for (const auto vertex : shape.vertices) {
					auto saved_vertex = vertices.append_map();
					saved_vertex.write("x", vertex.x);
					saved_vertex.write("y", vertex.y);
					saved_vertex.write("z", vertex.z);
				}
				auto indices = body.sequence("indices");
				for (const auto index : shape.indices) indices.append(index);
			}
		}, record.definition.shape);
		Serialization::write_vec3(body, "shape_offset", record.definition.shape_offset);
		body.write("motion", static_cast<int>(record.definition.motion));
		body.write("quality", static_cast<int>(record.definition.quality));
		body.write("participation", static_cast<int>(record.definition.participation));
		body.write("persistence", static_cast<int>(record.definition.persistence));
		body.write("mass", record.definition.mass);
		body.write("friction", record.definition.friction);
		body.write("restitution", record.definition.restitution);
		body.write("linear_damping", record.definition.linear_damping);
		body.write("angular_damping", record.definition.angular_damping);
		body.write("gravity_factor", record.definition.gravity_factor);
		body.write("enabled", api.IsAdded(record.body));
		body.write("active", api.IsActive(record.body));
		Serialization::write_vec3(body, "linear_velocity", to_glm(api.GetLinearVelocity(record.body)));
		Serialization::write_vec3(body, "angular_velocity", to_glm(api.GetAngularVelocity(record.body)));
	}
	auto character_controllers = system.sequence("character_controllers");
	for (const auto& [id, record] : impl->controllers) {
		if (get_ecs().has_transformation(id) && get_ecs().is_transient_transformation(id)) continue;
		auto character = character_controllers.append_map();
		character.write("entity_id", id.get_underlying());
		character.write("radius", record.definition.radius);
		character.write("height", record.definition.height);
		character.write("max_slope_angle", record.definition.max_slope_angle);
		character.write("step_height", record.definition.step_height);
		character.write("ground_snap_distance", record.definition.ground_snap_distance);
		Serialization::write_vec3(character, "desired_velocity", record.desired_velocity);
		Serialization::write_vec3(character, "linear_velocity", to_glm(record.character->GetLinearVelocity()));
	}
	auto overrides = system.sequence("contact_restitution_overrides");
	for (const auto& [pair, restitution] : impl->restitution_overrides) {
		const auto first = impl->bodies.find(pair.first), second = impl->bodies.find(pair.second);
		const bool first_persistent = first != impl->bodies.end()
			? first->second.definition.persistence == PhysicsPersistence::Persistent
			: impl->controllers.contains(pair.first);
		const bool second_persistent = second != impl->bodies.end()
			? second->second.definition.persistence == PhysicsPersistence::Persistent
			: impl->controllers.contains(pair.second);
		if (!first_persistent || !second_persistent
			|| (get_ecs().has_transformation(pair.first) && get_ecs().is_transient_transformation(pair.first))
			|| (get_ecs().has_transformation(pair.second) && get_ecs().is_transient_transformation(pair.second))) continue;
		auto entry = overrides.append_map();
		entry.write("first", pair.first.get_underlying());
		entry.write("second", pair.second.get_underlying());
		entry.write("restitution", restitution);
	}
}
void ::PhysicsSystem::deserialize(const Deserializer& in)
{
	const auto system = in.child("physics_system");
	auto replacement = std::make_unique<Impl>();
	const auto gravity = Serialization::read_vec3(system, "gravity");
	require_finite(gravity, system.path() + ".gravity");
	replacement->world.SetGravity(to_jolt(gravity));
	const auto entries = system.child("bodies").elements();
	std::unordered_map<EntityID, bool> seen;
	struct Restored { EntityID id; RigidBodyDefinition definition; glm::vec3 position; glm::quat rotation; glm::vec3 linear; glm::vec3 angular; bool enabled; bool active; ShapeRefC shape; };
	std::vector<Restored> restored;
	for (const auto& entry : entries) {
		const auto path = physics_entry_path(entry, "entity_id");
		const EntityID id(entry.read<std::uint64_t>("entity_id"));
		if (!seen.emplace(id, true).second) throw SerializationError("Duplicate physics entity at " + path);
		if (!get_ecs().has_transformation(id)) throw SerializationError("Physics body has no entity transformation at " + path);
		if (get_ecs().is_transient_transformation(id)) throw SerializationError("Persistent physics body conflicts with transient entity at " + path);
		const int shape = entry.read<int>("shape");
		if (shape < 0 || shape > 3) throw SerializationError("Invalid physics shape at " + physics_entry_path(entry, "shape"));
		RigidBodyDefinition d;
		if (shape == 0) d.shape = BoxPhysicsShape{Serialization::read_vec3(entry, "half_extents")};
		else if (shape == 1) d.shape = SpherePhysicsShape{entry.read<float>("radius")};
		else if (shape == 2) d.shape = CapsulePhysicsShape{entry.read<float>("radius"), entry.read<float>("height")};
		else {
			TriangleMeshPhysicsShape mesh;
			for (const auto& vertex : entry.child("vertices").elements())
				mesh.vertices.emplace_back(vertex.read<float>("x"), vertex.read<float>("y"), vertex.read<float>("z"));
			for (const auto& index : entry.child("indices").elements()) mesh.indices.push_back(index.as<uint32_t>());
			d.shape = std::move(mesh);
		}
		d.shape_offset = Serialization::read_vec3(entry, "shape_offset");
		require_finite(d.shape_offset, physics_entry_path(entry, "shape_offset"));
		d.motion = read_enum<PhysicsMotionType>(entry, "motion", 2, physics_entry_path(entry, "motion"));
		d.quality = read_enum<PhysicsMotionQuality>(entry, "quality", 1, physics_entry_path(entry, "quality"));
		d.participation = read_enum<PhysicsParticipation>(entry, "participation", 2, physics_entry_path(entry, "participation"));
		d.persistence = read_enum<PhysicsPersistence>(entry, "persistence", 1, physics_entry_path(entry, "persistence"));
		if (d.persistence == PhysicsPersistence::Transient) throw SerializationError("Transient physics body must be omitted at " + path);
		d.mass = entry.read<float>("mass"); d.friction = entry.read<float>("friction");
		d.restitution = entry.read<float>("restitution"); d.linear_damping = entry.read<float>("linear_damping");
		d.angular_damping = entry.read<float>("angular_damping"); d.gravity_factor = entry.read<float>("gravity_factor");
		d.enabled = entry.read<bool>("enabled");
		if (const auto box = std::get_if<BoxPhysicsShape>(&d.shape)) require_finite(box->half_extents, physics_entry_path(entry, "half_extents"));
		if (const auto sphere = std::get_if<SpherePhysicsShape>(&d.shape)) require_finite(sphere->radius, physics_entry_path(entry, "radius"));
		if (const auto capsule = std::get_if<CapsulePhysicsShape>(&d.shape)) { require_finite(capsule->radius, physics_entry_path(entry, "radius")); require_finite(capsule->height, physics_entry_path(entry, "height")); }
		if (const auto mesh = std::get_if<TriangleMeshPhysicsShape>(&d.shape)) {
			if (d.motion != PhysicsMotionType::Static)
				throw SerializationError("Triangle mesh physics shape must be static at " + physics_entry_path(entry, "motion"));
			for (const auto vertex : mesh->vertices) require_finite(vertex, physics_entry_path(entry, "vertices"));
		}
		for (const auto [value, field] : {std::pair{d.mass, "mass"}, {d.friction, "friction"}, {d.restitution, "restitution"}, {d.linear_damping, "linear_damping"}, {d.angular_damping, "angular_damping"}, {d.gravity_factor, "gravity_factor"}}) require_finite(value, physics_entry_path(entry, field));
		if (d.mass <= 0.0f || d.friction < 0.0f || d.restitution < 0.0f || d.linear_damping < 0.0f || d.angular_damping < 0.0f) throw SerializationError("Invalid physics body parameter at " + path);
		if (const auto box = std::get_if<BoxPhysicsShape>(&d.shape); box && (box->half_extents.x <= 0.0f || box->half_extents.y <= 0.0f || box->half_extents.z <= 0.0f)) throw SerializationError("Invalid box dimensions at " + path);
		if (const auto sphere = std::get_if<SpherePhysicsShape>(&d.shape); sphere && sphere->radius <= 0.0f) throw SerializationError("Invalid sphere radius at " + path);
		if (const auto capsule = std::get_if<CapsulePhysicsShape>(&d.shape); capsule && (capsule->radius <= 0.0f || capsule->height < 2.0f * capsule->radius)) throw SerializationError("Invalid capsule dimensions at " + path);
		ShapeRefC cooked_shape;
		try {
			cooked_shape = replacement->shape(d);
		} catch (const std::exception& error) {
			throw SerializationError("Invalid physics shape at " + physics_entry_path(entry, "shape") + ": " + error.what());
		}
		const glm::vec3 linear = Serialization::read_vec3(entry, "linear_velocity");
		const glm::vec3 angular = Serialization::read_vec3(entry, "angular_velocity");
		require_finite(linear, physics_entry_path(entry, "linear_velocity")); require_finite(angular, physics_entry_path(entry, "angular_velocity"));
		const glm::vec3 position = get_ecs().get_position(id);
		const glm::quat rotation = get_ecs().get_rotation(id);
		restored.push_back({id, d, position, rotation, linear, angular, d.enabled, entry.read<bool>("active"), std::move(cooked_shape)});
	}
	struct RestoredController { EntityID id; CharacterControllerDefinition definition; glm::vec3 position; glm::vec3 desired; glm::vec3 linear; };
	std::vector<RestoredController> restored_controllers;
	const auto system_keys = system.keys();
	if (std::find(system_keys.begin(), system_keys.end(), "character_controllers") != system_keys.end()) {
		for (const auto& entry : system.child("character_controllers").elements()) {
			const auto path = physics_entry_path(entry, "entity_id");
			const EntityID id(entry.read<std::uint64_t>("entity_id"));
			if (!seen.emplace(id, true).second) throw SerializationError("Duplicate physics entity at " + path);
			if (!get_ecs().has_transformation(id)) throw SerializationError("Character controller has no entity transformation at " + path);
			if (get_ecs().is_transient_transformation(id)) throw SerializationError("Persistent character controller conflicts with transient entity at " + path);
			CharacterControllerDefinition definition;
			definition.radius = entry.read<float>("radius");
			definition.height = entry.read<float>("height");
			definition.max_slope_angle = entry.read<float>("max_slope_angle");
			definition.step_height = entry.read<float>("step_height");
			definition.ground_snap_distance = entry.read<float>("ground_snap_distance");
			try { validate_controller_definition(definition); }
			catch (const std::exception& error) { throw SerializationError("Invalid character controller at " + path + ": " + error.what()); }
			const glm::vec3 desired = Serialization::read_vec3(entry, "desired_velocity");
			const glm::vec3 linear = Serialization::read_vec3(entry, "linear_velocity");
			require_finite(desired, physics_entry_path(entry, "desired_velocity"));
			require_finite(linear, physics_entry_path(entry, "linear_velocity"));
			if (std::abs(desired.y) > 0.0001f) throw SerializationError("Character desired velocity must be planar at " + physics_entry_path(entry, "desired_velocity"));
			restored_controllers.push_back({id, definition, get_ecs().get_position(id), desired, linear});
		}
	}
	for (const auto& state : restored) {
		const auto old = impl->bodies.find(state.id);
		if ((old != impl->bodies.end() && old->second.definition.persistence == PhysicsPersistence::Transient)
			|| (get_ecs().has_transformation(state.id) && get_ecs().is_transient_transformation(state.id)))
			throw SerializationError("Persistent physics body conflicts with transient body at " + std::to_string(state.id.get_underlying()));
	}
	for (const auto& state : restored_controllers) {
		const auto old = impl->bodies.find(state.id);
		if ((old != impl->bodies.end() && old->second.definition.persistence == PhysicsPersistence::Transient)
			|| (get_ecs().has_transformation(state.id) && get_ecs().is_transient_transformation(state.id)))
			throw SerializationError("Persistent character controller conflicts with transient body at " + std::to_string(state.id.get_underlying()));
	}
	std::unordered_map<Impl::EntityPair, float, Impl::EntityPairHash> overrides;
	for (const auto& entry : system.child("contact_restitution_overrides").elements()) {
		const EntityID first(entry.read<std::uint64_t>("first")), second(entry.read<std::uint64_t>("second"));
		const float restitution = entry.read<float>("restitution");
		require_finite(restitution, entry.path() + ".restitution");
		if (restitution < 0.0f || first == second || !seen.contains(first) || !seen.contains(second)) throw SerializationError("Invalid contact restitution override at " + entry.path());
		const auto pair = Impl::ordered_pair(first, second);
		if (!overrides.emplace(pair, restitution).second) throw SerializationError("Duplicate contact restitution override at " + entry.path());
	}
	std::vector<Restored> transient;
	for (const auto& [id, record] : impl->bodies) {
		if (record.definition.persistence != PhysicsPersistence::Transient && (!get_ecs().has_transformation(id) || !get_ecs().is_transient_transformation(id))) continue;
		const auto& api = impl->world.GetBodyInterface();
		const auto q = api.GetRotation(record.body);
		transient.push_back({id, record.definition, to_glm(Vec3(api.GetPosition(record.body))), {q.GetW(), q.GetX(), q.GetY(), q.GetZ()}, to_glm(api.GetLinearVelocity(record.body)), to_glm(api.GetAngularVelocity(record.body)), api.IsAdded(record.body), api.IsActive(record.body), impl->shape(record.definition)});
	}
	std::vector<RestoredController> transient_controllers;
	for (const auto& [id, record] : impl->controllers) {
		if (get_ecs().has_transformation(id) && !get_ecs().is_transient_transformation(id)) continue;
		transient_controllers.push_back({id, record.definition, to_glm(Vec3(record.character->GetPosition())),
			record.desired_velocity, to_glm(record.character->GetLinearVelocity())});
	}
	auto install = [&](const Restored& state) {
		replacement->add_body(state.id, state.definition, state.shape, state.position, state.rotation);
		auto& record = replacement->bodies.at(state.id);
		auto& api = replacement->world.GetBodyInterface();
		if (!state.enabled && api.IsAdded(record.body)) api.RemoveBody(record.body);
		api.SetPositionAndRotation(record.body, to_jolt_r(state.position), to_jolt(state.rotation), EActivation::DontActivate);
		if (state.definition.motion != PhysicsMotionType::Static) {
			api.SetLinearVelocity(record.body, to_jolt(state.linear)); api.SetAngularVelocity(record.body, to_jolt(state.angular));
		}
		if (state.enabled && state.definition.motion != PhysicsMotionType::Static) {
			if (state.active) api.ActivateBody(record.body);
			else api.DeactivateBody(record.body);
		}
	};
	for (const auto& state : transient) install(state);
	for (const auto& state : restored) install(state);
	for (const auto& state : transient_controllers) {
		replacement->add_controller(state.id, state.definition, state.position);
		auto& record = replacement->controllers.at(state.id);
		record.desired_velocity = state.desired;
		record.character->SetLinearVelocity(to_jolt(state.linear));
		record.launch_locked = state.linear.y > 0.0f;
	}
	for (const auto& state : restored_controllers) {
		replacement->add_controller(state.id, state.definition, state.position);
		auto& record = replacement->controllers.at(state.id);
		record.desired_velocity = state.desired;
		record.character->SetLinearVelocity(to_jolt(state.linear));
		record.launch_locked = state.linear.y > 0.0f;
	}
	impl = std::move(replacement);
	impl->restitution_overrides = std::move(overrides);
}
