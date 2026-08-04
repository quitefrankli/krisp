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
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <algorithm>
#include <cmath>
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
using JPH::cMaxPhysicsBarriers; using JPH::cMaxPhysicsJobs;

namespace {
constexpr ObjectLayer STATIC_LAYER = 0;
constexpr ObjectLayer MOVING_LAYER = 1;
constexpr ObjectLayer SENSOR_LAYER = 2;
constexpr uint NUM_LAYERS = 3;
constexpr BroadPhaseLayer BP_STATIC(0), BP_MOVING(1), BP_SENSOR(2);

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
		auto& bodies_api = world.GetBodyInterface();
		for (const auto& [_, r] : bodies) {
			if (bodies_api.IsAdded(r.body)) bodies_api.RemoveBody(r.body);
			bodies_api.DestroyBody(r.body);
		}
	}

	ShapeRefC shape(const RigidBodyDefinition& d)
	{
		return std::visit([](const auto& s) -> ShapeRefC {
			using T = std::decay_t<decltype(s)>;
			if constexpr (std::is_same_v<T, BoxPhysicsShape>) return new BoxShape(to_jolt(s.half_extents));
			else if constexpr (std::is_same_v<T, SpherePhysicsShape>) return new SphereShape(s.radius);
			else return new CapsuleShape(std::max(0.0f, s.height * 0.5f - s.radius), s.radius);
		}, d.shape);
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
	remove_rigid_body(id);
	if (!get_ecs().has_transformation(id)) throw std::invalid_argument("Rigid body entity has no transformation");
	const auto position = get_ecs().get_position(id);
	const auto rotation = get_ecs().get_rotation(id);
	BodyCreationSettings settings(impl->shape(d), to_jolt_r(position), to_jolt(rotation), motion_for(d.motion), layer_for(d));
	settings.mUserData = id.get_underlying();
	settings.mFriction = d.friction;
	settings.mRestitution = d.restitution;
	settings.mLinearDamping = d.linear_damping;
	settings.mAngularDamping = d.angular_damping;
	settings.mGravityFactor = d.gravity_factor;
	settings.mMotionQuality = d.quality == PhysicsMotionQuality::Continuous ? EMotionQuality::LinearCast : EMotionQuality::Discrete;
	settings.mIsSensor = d.participation == PhysicsParticipation::Sensor;
	if (d.motion == PhysicsMotionType::Dynamic) settings.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia, settings.mMassPropertiesOverride.mMass = d.mass;
	auto& api = impl->world.GetBodyInterface();
	Body* body = api.CreateBody(settings);
	if (!body) throw std::runtime_error("Jolt body capacity exhausted");
	impl->bodies.emplace(id, Impl::BodyRecord{body->GetID(), d, position, rotation});
	if (d.enabled) api.AddBody(body->GetID(), d.motion == PhysicsMotionType::Dynamic ? EActivation::Activate : EActivation::DontActivate);
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
		std::vector<EntityID> candidates; for (const auto& [candidate, _] : impl->bodies) if (candidate != id) candidates.push_back(candidate);
		return raycast(ray, candidates);
	}
	return {true, id, ray.origin + ray.direction * (10000.0f * hit.mFraction)};
}
DetectedEntityCollision PhysicsSystem::raycast(const Maths::Ray& ray, std::span<const EntityID> candidates) const
{
	DetectedEntityCollision best; float distance = 10000.0f;
	for (EntityID id : candidates) {
		auto it = impl->bodies.find(id); if (it == impl->bodies.end()) continue;
		RayCastResult hit;
		BodyLockRead lock(impl->world.GetBodyLockInterface(), it->second.body);
		if (!lock.Succeeded()) continue;
		const Body& body = lock.GetBody();
		const Quat inverse = body.GetRotation().Conjugated();
		const RayCast cast(inverse * (to_jolt(ray.origin) - Vec3(body.GetPosition())), inverse * to_jolt(ray.direction * distance));
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
	bodies.reserve(impl->bodies.size());
	for (const auto& [entity, record] : impl->bodies) {
		BodyLockRead lock(impl->world.GetBodyLockInterface(), record.body);
		if (!lock.Succeeded() || !lock.GetBody().IsInBroadPhase()) continue;
		const Body& body = lock.GetBody();
		const Quat rotation = body.GetRotation();
		bodies.push_back({entity, record.body.GetIndexAndSequenceNumber(),
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
	const auto found = impl->bodies.find(id);
	if (found == impl->bodies.end()) return triangles;
	BodyLockRead lock(impl->world.GetBodyLockInterface(), found->second.body);
	if (!lock.Succeeded()) return triangles;
	const auto* shape = lock.GetBody().GetShape();
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
	auto system = out.map("physics_system"); Serialization::write_vec3(system, "gravity", get_gravity());
}
void ::PhysicsSystem::deserialize(const Deserializer& in)
{
	impl = std::make_unique<Impl>(); set_gravity(Serialization::read_vec3(in.child("physics_system"), "gravity"));
}
