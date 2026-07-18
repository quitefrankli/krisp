#include <entity_component_system/ecs.hpp>
#include <serialization/serializer.hpp>

#include <gtest/gtest.h>

#include <limits>
#include <memory>
#include <stdexcept>

namespace
{
class UnsupportedForceSystem : public ForceSystem
{
public:
	void compute_forces(float, std::unordered_map<ObjectID, PhysicsComponent>&) override {}
};

class UnsupportedCollider : public Collider
{
public:
	ECollider get_type() const override { return static_cast<ECollider>(255); }
	Object& spawn_debug_object(GameEngine&) const override { throw std::runtime_error("unused"); }
	void update_debug_object(Object&) const override {}
};
}

TEST(EcsSerialization, round_trips_selected_systems_as_an_exact_checkpoint)
{
	ECS source;
	const EntityID ray_id(10);
	const EntityID sphere_id(11);
	const EntityID quad_id(12);
	const EntityID box_id(13);
	const EntityID mesh_id(14);
	const EntityID clickable_id(100);
	const EntityID hoverable_id(101);

	Maths::Ray ray({ 1.0f, 2.0f, 3.0f }, { 0.0f, 1.0f, 0.0f });
	ray.length = 9.0f;
	source.add_collider(ray_id, std::make_unique<RayCollider>(ray));
	source.add_collider(sphere_id, std::make_unique<SphereCollider>(Maths::Sphere({ 4.0f, 5.0f, 6.0f }, 2.5f)));
	source.add_collider(quad_id, std::make_unique<QuadCollider>(
		Maths::Plane({ 7.0f, 8.0f, 9.0f }, { 0.0f, 0.0f, 1.0f }), glm::vec2(3.0f, 4.0f)));
	source.add_collider(box_id, std::make_unique<BoxCollider>(AABB({ -2.0f, -3.0f, -4.0f }, { 2.0f, 3.0f, 4.0f })));
	source.add_collider(mesh_id, std::make_unique<MeshCollider>(std::vector<MeshID>{ MeshID(21), MeshID(22) }));
	source.add_clickable_entity(clickable_id);
	source.add_hoverable_entity(hoverable_id);
	source.add_light_source(mesh_id, LightComponent{ .intensity = 3.5f, .color = { 0.1f, 0.2f, 0.3f } });

	PhysicsComponent physics;
	physics.mass = 17.0f;
	physics.position = { 1.0f, 2.0f, 3.0f };
	physics.velocity = { 4.0f, 5.0f, 6.0f };
	physics.angular_velocity = glm::quat(0.5f, 0.1f, 0.2f, 0.3f);
	physics.acceleration = { 7.0f, 8.0f, 9.0f };
	physics._net_force = { 10.0f, 11.0f, 12.0f };
	source.add_physics_entity(ray_id, physics);
	source.get_gravity_system().set_gravity_type(GravitySystem::GravityType::TRUE);

	Serializer serializer;
	source.serialize(serializer);
	const auto serialized = Deserializer::parse(serializer.emit());
	const auto serialized_colliders = serialized.child("collider_system").elements();
	ASSERT_EQ(serialized_colliders.size(), 5);
	for (std::size_t index = 0; index < serialized_colliders.size(); ++index)
		EXPECT_EQ(serialized_colliders[index].read<std::uint64_t>("entity_id"), 10 + index);

	ECS restored;
	restored.add_light_source(EntityID(99), LightComponent{});
	restored.deserialize(serialized);

	const auto* light = restored.get_light_component(mesh_id);
	ASSERT_NE(light, nullptr);
	EXPECT_FLOAT_EQ(light->intensity, 3.5f);
	EXPECT_EQ(light->color, glm::vec3(0.1f, 0.2f, 0.3f));
	EXPECT_EQ(restored.get_light_component(EntityID(99)), nullptr);

	const auto& colliders = restored.get_all_colliders();
	ASSERT_EQ(colliders.size(), 5);
	const auto restored_ray = dynamic_cast<const RayCollider*>(colliders.at(ray_id).collider.get())->get_local_data();
	EXPECT_EQ(restored_ray.origin, ray.origin);
	EXPECT_EQ(restored_ray.direction, ray.direction);
	EXPECT_FLOAT_EQ(restored_ray.length, ray.length);
	const auto restored_sphere = dynamic_cast<const SphereCollider*>(colliders.at(sphere_id).collider.get())->get_local_data();
	EXPECT_EQ(restored_sphere.origin, glm::vec3(4.0f, 5.0f, 6.0f));
	EXPECT_FLOAT_EQ(restored_sphere.radius, 2.5f);
	const auto restored_quad = dynamic_cast<const QuadCollider*>(colliders.at(quad_id).collider.get())->get_local_data();
	EXPECT_EQ(restored_quad.offset, glm::vec3(7.0f, 8.0f, 9.0f));
	EXPECT_EQ(restored_quad.size, glm::vec2(3.0f, 4.0f));
	const auto restored_box = dynamic_cast<const BoxCollider*>(colliders.at(box_id).collider.get())->get_local_data();
	EXPECT_EQ(restored_box.min_bound, glm::vec3(-2.0f, -3.0f, -4.0f));
	EXPECT_EQ(restored_box.max_bound, glm::vec3(2.0f, 3.0f, 4.0f));
	EXPECT_EQ(dynamic_cast<const MeshCollider*>(colliders.at(mesh_id).collider.get())->get_mesh_ids(),
		(std::vector<MeshID>{ MeshID(21), MeshID(22) }));

	const auto* restored_physics = restored._get_physics_component(ray_id);
	ASSERT_NE(restored_physics, nullptr);
	EXPECT_FLOAT_EQ(restored_physics->mass, physics.mass);
	EXPECT_EQ(restored_physics->position, physics.position);
	EXPECT_EQ(restored_physics->velocity, physics.velocity);
	EXPECT_EQ(restored_physics->angular_velocity, physics.angular_velocity);
	EXPECT_EQ(restored_physics->acceleration, physics.acceleration);
	EXPECT_EQ(restored_physics->_net_force, physics._net_force);
	EXPECT_EQ(restored.get_gravity_system().get_gravity_type(), GravitySystem::GravityType::TRUE);

	Serializer restored_serializer;
	restored.serialize(restored_serializer);
	const auto restored_document = Deserializer::parse(restored_serializer.emit());
	EXPECT_EQ(restored_document.child("clickable_system").elements()[0].read<std::uint64_t>("entity_id"),
		clickable_id.get_underlying());
	EXPECT_EQ(restored_document.child("hoverable_system").elements()[0].read<std::uint64_t>("entity_id"),
		hoverable_id.get_underlying());
}

TEST(EcsSerialization, individual_deserialization_is_atomic_and_reports_paths)
{
	ECS ecs;
	ecs.add_light_source(EntityID(1), LightComponent{ .intensity = 8.0f });
	const auto malformed = Deserializer::parse(
		"light_system:\n"
		"  - entity_id: 2\n"
		"    intensity: wrong\n"
		"    color: {x: 1, y: 1, z: 1}\n");
	try {
		ecs.LightSystem::deserialize(malformed);
		FAIL() << "Expected malformed light to fail";
	} catch (const SerializationError& error) {
		EXPECT_NE(std::string(error.what()).find("$.light_system[0].intensity"), std::string::npos);
	}
	EXPECT_NE(ecs.get_light_component(EntityID(1)), nullptr);
	EXPECT_EQ(ecs.get_light_component(EntityID(2)), nullptr);

	const auto duplicate = Deserializer::parse(
		"light_system:\n"
		"  - {entity_id: 2, intensity: 1, color: {x: 1, y: 1, z: 1}}\n"
		"  - {entity_id: 2, intensity: 2, color: {x: 2, y: 2, z: 2}}\n");
	try {
		ecs.LightSystem::deserialize(duplicate);
		FAIL() << "Expected duplicate entity ID to fail";
	} catch (const SerializationError& error) {
		EXPECT_NE(std::string(error.what()).find("$.light_system[1].entity_id"), std::string::npos);
	}
	EXPECT_NE(ecs.get_light_component(EntityID(1)), nullptr);
}

TEST(EcsSerialization, rejects_unsupported_polymorphic_types)
{
	ECS ecs;
	ecs.add_collider(EntityID(1), std::make_unique<UnsupportedCollider>());
	Serializer collider_serializer;
	try {
		ecs.ColliderSystem::serialize(collider_serializer);
		FAIL() << "Expected unsupported collider to fail";
	} catch (const SerializationError& error) {
		EXPECT_NE(std::string(error.what()).find("$.collider_system[0].type"), std::string::npos);
	}
	const auto unknown_collider = Deserializer::parse(
		"collider_system:\n"
		"  - {entity_id: 2, type: capsule, data: {}}\n");
	try {
		ecs.ColliderSystem::deserialize(unknown_collider);
		FAIL() << "Expected unknown collider type to fail";
	} catch (const SerializationError& error) {
		EXPECT_NE(std::string(error.what()).find("$.collider_system[0].type"), std::string::npos);
	}
	EXPECT_TRUE(ecs.get_all_colliders().contains(EntityID(1)));

	ECS physics_ecs;
	physics_ecs.add_force_system(std::make_unique<UnsupportedForceSystem>());
	Serializer physics_serializer;
	try {
		physics_ecs.PhysicsSystem::serialize(physics_serializer);
		FAIL() << "Expected unsupported force system to fail";
	} catch (const SerializationError& error) {
		EXPECT_NE(std::string(error.what()).find("$.physics_system.force_systems[1].type"), std::string::npos);
	}
}

TEST(HoverableSystemSerialization, round_trip_replaces_and_sorts_entities)
{
	ECS source;
	source.add_hoverable_entity(EntityID(8));
	source.add_hoverable_entity(EntityID(3));
	Serializer serializer;
	source.HoverableSystem::serialize(serializer);

	ECS restored;
	restored.add_hoverable_entity(EntityID(99));
	restored.HoverableSystem::deserialize(Deserializer::parse(serializer.emit()));
	Serializer output;
	restored.HoverableSystem::serialize(output);
	const auto entries = Deserializer::parse(output.emit()).child("hoverable_system").elements();
	ASSERT_EQ(entries.size(), 2);
	EXPECT_EQ(entries[0].read<std::uint64_t>("entity_id"), 3);
	EXPECT_EQ(entries[1].read<std::uint64_t>("entity_id"), 8);
}

TEST(HoverableSystemSerialization, duplicate_ids_fail_atomically_with_path)
{
	ECS ecs;
	ecs.add_hoverable_entity(EntityID(7));
	const auto duplicate = Deserializer::parse(
		"hoverable_system:\n  - {entity_id: 2}\n  - {entity_id: 2}\n");
	try {
		ecs.HoverableSystem::deserialize(duplicate);
		FAIL() << "Expected duplicate hoverable ID to fail";
	} catch (const SerializationError& error) {
		EXPECT_NE(std::string(error.what()).find("$.hoverable_system[1].entity_id"), std::string::npos);
	}
	Serializer output;
	ecs.HoverableSystem::serialize(output);
	const auto entries = Deserializer::parse(output.emit()).child("hoverable_system").elements();
	ASSERT_EQ(entries.size(), 1);
	EXPECT_EQ(entries[0].read<std::uint64_t>("entity_id"), 7);
}

TEST(LightSystemSerialization, round_trip_replaces_complete_component)
{
	ECS source;
	const EntityID id(std::numeric_limits<std::uint64_t>::max());
	source.add_light_source(id, LightComponent{ .intensity = 4.25f, .color = { 0.2f, 0.4f, 0.8f } });
	Serializer serializer;
	source.LightSystem::serialize(serializer);

	ECS restored;
	restored.add_light_source(EntityID(1), LightComponent{});
	restored.LightSystem::deserialize(Deserializer::parse(serializer.emit()));
	const auto* light = restored.get_light_component(id);
	ASSERT_NE(light, nullptr);
	EXPECT_FLOAT_EQ(light->intensity, 4.25f);
	EXPECT_EQ(light->color, glm::vec3(0.2f, 0.4f, 0.8f));
	EXPECT_EQ(restored.get_light_component(EntityID(1)), nullptr);
}

TEST(LightSystemSerialization, malformed_component_fails_atomically_with_path)
{
	ECS ecs;
	ecs.add_light_source(EntityID(1), LightComponent{ .intensity = 6.0f });
	const auto malformed = Deserializer::parse(
		"light_system:\n  - {entity_id: 2, intensity: bad, color: {x: 1, y: 1, z: 1}}\n");
	try {
		ecs.LightSystem::deserialize(malformed);
		FAIL() << "Expected malformed light to fail";
	} catch (const SerializationError& error) {
		EXPECT_NE(std::string(error.what()).find("$.light_system[0].intensity"), std::string::npos);
	}
	EXPECT_NE(ecs.get_light_component(EntityID(1)), nullptr);
	EXPECT_EQ(ecs.get_light_component(EntityID(2)), nullptr);
}

TEST(ColliderSystemSerialization, round_trip_replaces_local_geometry)
{
	ECS source;
	source.add_collider(EntityID(2), std::make_unique<SphereCollider>(
		Maths::Sphere({ 1.0f, 2.0f, 3.0f }, 4.0f)));
	Serializer serializer;
	source.ColliderSystem::serialize(serializer);

	ECS restored;
	restored.add_collider(EntityID(1), std::make_unique<BoxCollider>());
	restored.ColliderSystem::deserialize(Deserializer::parse(serializer.emit()));
	ASSERT_EQ(restored.get_all_colliders().size(), 1);
	const auto* sphere = dynamic_cast<const SphereCollider*>(
		restored.get_all_colliders().at(EntityID(2)).collider.get());
	ASSERT_NE(sphere, nullptr);
	EXPECT_EQ(sphere->get_local_data().origin, glm::vec3(1.0f, 2.0f, 3.0f));
	EXPECT_FLOAT_EQ(sphere->get_local_data().radius, 4.0f);
}

TEST(ColliderSystemSerialization, duplicate_ids_fail_atomically_with_path)
{
	ECS ecs;
	ecs.add_collider(EntityID(1), std::make_unique<BoxCollider>());
	const auto duplicate = Deserializer::parse(
		"collider_system:\n"
		"  - {entity_id: 2, type: sphere, data: {origin: {x: 0, y: 0, z: 0}, radius: 1}}\n"
		"  - {entity_id: 2, type: sphere, data: {origin: {x: 0, y: 0, z: 0}, radius: 1}}\n");
	try {
		ecs.ColliderSystem::deserialize(duplicate);
		FAIL() << "Expected duplicate collider ID to fail";
	} catch (const SerializationError& error) {
		EXPECT_NE(std::string(error.what()).find("$.collider_system[1].entity_id"), std::string::npos);
	}
	EXPECT_TRUE(ecs.get_all_colliders().contains(EntityID(1)));
	EXPECT_FALSE(ecs.get_all_colliders().contains(EntityID(2)));
}

TEST(PhysicsSystemSerialization, round_trip_replaces_complete_runtime_state)
{
	ECS source;
	PhysicsComponent component;
	component.mass = 9.0f;
	component.velocity = { 1.0f, 2.0f, 3.0f };
	component.acceleration = { 4.0f, 5.0f, 6.0f };
	component._net_force = { 7.0f, 8.0f, 9.0f };
	source.add_physics_entity(EntityID(2), component);
	source.get_gravity_system().set_gravity_type(GravitySystem::GravityType::DISABLED);
	Serializer serializer;
	source.PhysicsSystem::serialize(serializer);

	ECS restored;
	restored.add_physics_entity(EntityID(1), PhysicsComponent{});
	restored.PhysicsSystem::deserialize(Deserializer::parse(serializer.emit()));
	const auto* result = restored._get_physics_component(EntityID(2));
	ASSERT_NE(result, nullptr);
	EXPECT_FLOAT_EQ(result->mass, 9.0f);
	EXPECT_EQ(result->velocity, component.velocity);
	EXPECT_EQ(result->acceleration, component.acceleration);
	EXPECT_EQ(result->_net_force, component._net_force);
	EXPECT_EQ(restored._get_physics_component(EntityID(1)), nullptr);
	EXPECT_EQ(restored.get_gravity_system().get_gravity_type(), GravitySystem::GravityType::DISABLED);
}

TEST(PhysicsSystemSerialization, unknown_force_fails_atomically_with_path)
{
	ECS ecs;
	ecs.add_physics_entity(EntityID(1), PhysicsComponent{ .mass = 5.0f });
	ecs.get_gravity_system().set_gravity_type(GravitySystem::GravityType::TRUE);
	const auto malformed = Deserializer::parse(
		"physics_system:\n"
		"  components: []\n"
		"  force_systems:\n"
		"    - {type: custom, gravity_type: disabled}\n");
	try {
		ecs.PhysicsSystem::deserialize(malformed);
		FAIL() << "Expected unknown force system to fail";
	} catch (const SerializationError& error) {
		EXPECT_NE(std::string(error.what()).find("$.physics_system.force_systems[0].type"), std::string::npos);
	}
	ASSERT_NE(ecs._get_physics_component(EntityID(1)), nullptr);
	EXPECT_FLOAT_EQ(ecs._get_physics_component(EntityID(1))->mass, 5.0f);
	EXPECT_EQ(ecs.get_gravity_system().get_gravity_type(), GravitySystem::GravityType::TRUE);
}
