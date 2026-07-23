#include <entity_component_system/ecs.hpp>
#include <serialization/serializer.hpp>

#include <gtest/gtest.h>
#include <glm/gtc/matrix_transform.hpp>

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
	Object aggregate_object;
	source.add_object(aggregate_object);
	Maths::Transform animation_finish;
	animation_finish.set_pos({ 5.0f, 0.0f, 0.0f });
	source.animate(aggregate_object.get_id(), AnimationSequence(Maths::Transform{}, animation_finish, 2.0f));
	source.spawn_tileset(0, 0, 2.0f, "aggregate");
	source.move_to_tile({ 2, 3 }, aggregate_object.get_id(), "aggregate");
	Bone aggregate_bone;
	aggregate_bone.name = "aggregate_root";
	const auto aggregate_skeleton = source.add_skeleton({ aggregate_bone });
	const auto aggregate_animation = source.add_skeletal_animation(
		"aggregate_idle", { BoneAnimation{} }, make_skeletal_rig_signature({ aggregate_bone }));

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
	EXPECT_EQ(restored.get_skeletal_component(aggregate_skeleton).get_bones()[0].name, "aggregate_root");
	EXPECT_TRUE(restored.get_skeletal_animations().contains(aggregate_animation));
	EXPECT_EQ(restored.get_tile_coord(aggregate_object.get_id(), "aggregate"), TileCoord(2, 3));

	Serializer restored_serializer;
	restored.serialize(restored_serializer);
	const auto restored_document = Deserializer::parse(restored_serializer.emit());
	EXPECT_EQ(restored_document.child("animation_system").elements().size(), 1);
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

TEST(ColliderSystemSerialization, transient_colliders_are_not_serialized)
{
	ECS ecs;
	ecs.add_collider(EntityID(1), std::make_unique<BoxCollider>());
	ecs.add_collider(EntityID(2), std::make_unique<BoxCollider>(), {}, ColliderPersistence::Transient);
	Serializer serializer;
	ecs.ColliderSystem::serialize(serializer);

	ECS restored;
	restored.ColliderSystem::deserialize(Deserializer::parse(serializer.emit()));
	EXPECT_TRUE(restored.get_all_colliders().contains(EntityID(1)));
	EXPECT_FALSE(restored.get_all_colliders().contains(EntityID(2)));
}

TEST(ColliderSystemSerialization, deserialization_preserves_transient_colliders)
{
	ECS source;
	source.add_collider(EntityID(2), std::make_unique<SphereCollider>());
	Serializer serializer;
	source.ColliderSystem::serialize(serializer);

	ECS restored;
	restored.add_collider(EntityID(1), std::make_unique<BoxCollider>(), {}, ColliderPersistence::Transient);
	restored.add_collider(EntityID(3), std::make_unique<BoxCollider>());
	restored.ColliderSystem::deserialize(Deserializer::parse(serializer.emit()));

	EXPECT_TRUE(restored.get_all_colliders().contains(EntityID(1)));
	EXPECT_TRUE(restored.get_all_colliders().contains(EntityID(2)));
	EXPECT_FALSE(restored.get_all_colliders().contains(EntityID(3)));
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

TEST(AnimationSystemSerialization, resumes_elapsed_animation_after_round_trip)
{
	ECS source;
	Object object;
	source.add_object(object);
	Maths::Transform start;
	Maths::Transform finish;
	finish.set_pos({ 8.0f, 0.0f, 0.0f });
	source.animate(object.get_id(), AnimationSequence(start, finish, 2.0f));
	source.process(0.5f);
	Serializer serializer;
	source.AnimationSystem::serialize(serializer);

	object.set_position(Maths::zero_vec);
	ECS restored;
	restored.add_object(object);
	restored.AnimationSystem::deserialize(Deserializer::parse(serializer.emit()));
	restored.process(0.5f);
	EXPECT_FLOAT_EQ(object.get_position().x, 4.0f);
}

TEST(AnimationSystemSerialization, malformed_state_fails_atomically)
{
	ECS source;
	Object object;
	source.add_object(object);
	Maths::Transform finish;
	finish.set_pos({ 2.0f, 0.0f, 0.0f });
	source.animate(object.get_id(), AnimationSequence(Maths::Transform{}, finish, 1.0f));
	Serializer serializer;
	source.AnimationSystem::serialize(serializer);
	std::string malformed = serializer.emit();
	malformed.replace(malformed.find("duration_secs: 1"), std::string("duration_secs: 1").size(), "duration_secs: bad");
	EXPECT_THROW(source.AnimationSystem::deserialize(Deserializer::parse(malformed)), SerializationError);
	Serializer retained;
	source.AnimationSystem::serialize(retained);
	EXPECT_EQ(Deserializer::parse(retained.emit()).child("animation_system").elements().size(), 1);
}

TEST(SkeletalSystemSerialization, round_trips_bone_hierarchy_and_matrices)
{
	ECS source;
	Bone root;
	root.name = "root";
	root.inverse_bind_pose.set_mat4(glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 2.0f, 3.0f)));
	Bone child;
	child.name = "child";
	child.parent_node = 0;
	child.relative_transform.set_pos({ 0.0f, 4.0f, 0.0f });
	const auto id = source.add_skeleton({ root, child });
	const ObjectID entity(987);
	source.attach_skeleton(entity, id);
	Serializer serializer;
	source.SkeletalSystem::serialize(serializer);

	ECS restored;
	restored.SkeletalSystem::deserialize(Deserializer::parse(serializer.emit()));
	const auto& bones = restored.get_skeletal_component(id).get_bones();
	ASSERT_EQ(bones.size(), 2);
	EXPECT_EQ(bones[1].parent_node, 0);
	EXPECT_EQ(bones[1].name, "child");
	EXPECT_EQ(bones[0].inverse_bind_pose.get_mat4(), root.inverse_bind_pose.get_mat4());
	EXPECT_EQ(restored.get_skeleton_id(entity), id);
}

TEST(SkeletalSystemSerialization, invalid_parent_fails_atomically_with_path)
{
	ECS ecs;
	Bone bone;
	const auto id = ecs.add_skeleton({ bone });
	Serializer serializer;
	ecs.SkeletalSystem::serialize(serializer);
	std::string malformed = serializer.emit();
	malformed.replace(malformed.find("parent_index: ~"), std::string("parent_index: ~").size(), "parent_index: 9");
	try {
		ecs.SkeletalSystem::deserialize(Deserializer::parse(malformed));
		FAIL() << "Expected invalid bone parent to fail";
	} catch (const SerializationError& error) {
		EXPECT_NE(std::string(error.what()).find("parent_index"), std::string::npos);
	}
	EXPECT_EQ(ecs.get_skeletal_component(id).get_bones().size(), 1);
}

TEST(SkeletalAnimationSystemSerialization, round_trips_tracks_and_active_playback)
{
	ECS source;
	Bone bone;
	bone.name = "root";
	const auto skeleton_id = source.add_skeleton({ bone });
	BoneAnimation animation;
	animation.animation_start_secs = 0.0f;
	animation.animation_end_secs = 1.0f;
	animation.translation_track.keys = {
		{ 0.0f, glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(0.0f) },
		{ 1.0f, glm::vec3(2.0f, 0.0f, 0.0f), glm::vec3(0.0f), glm::vec3(0.0f) }
	};
	const auto rig = make_skeletal_rig_signature({ bone });
	const auto animation_id = source.add_skeletal_animation("walk", { animation }, rig, "model.glb");
	source.play_animation(skeleton_id, animation_id, true);
	source.set_animation_speed(skeleton_id, 0.5f);
	source.SkeletalAnimationSystem::process(0.25f);
	Serializer skeletons;
	Serializer animations;
	source.SkeletalSystem::serialize(skeletons);
	source.SkeletalAnimationSystem::serialize(animations);

	ECS restored;
	restored.SkeletalSystem::deserialize(Deserializer::parse(skeletons.emit()));
	restored.SkeletalAnimationSystem::deserialize(Deserializer::parse(animations.emit()));
	ASSERT_TRUE(restored.get_skeletal_animations().contains(animation_id));
	EXPECT_EQ(restored.get_skeletal_animations().at(animation_id).source, "model.glb");
	ASSERT_TRUE(restored.get_animation_playback(skeleton_id));
	EXPECT_FLOAT_EQ(restored.get_animation_playback(skeleton_id)->speed, 0.5f);
	restored.SkeletalAnimationSystem::process(0.25f);
	EXPECT_FLOAT_EQ(restored.get_skeletal_component(skeleton_id).get_bones()[0].relative_transform.get_pos().x, 0.5f);
}

TEST(SkeletalAnimationSystemSerialization, unknown_interpolation_fails_atomically)
{
	ECS ecs;
	Bone bone;
	bone.name = "root";
	BoneAnimation animation;
	animation.translation_track.keys.push_back({});
	const auto id = ecs.add_skeletal_animation("idle", { animation }, make_skeletal_rig_signature({ bone }));
	Serializer serializer;
	ecs.SkeletalAnimationSystem::serialize(serializer);
	std::string malformed = serializer.emit();
	malformed.replace(malformed.find("interpolation: linear"), std::string("interpolation: linear").size(),
		"interpolation: unknown");
	EXPECT_THROW(ecs.SkeletalAnimationSystem::deserialize(Deserializer::parse(malformed)), SerializationError);
	EXPECT_TRUE(ecs.get_skeletal_animations().contains(id));
}

TEST(TileSystemSerialization, round_trips_topology_without_registered_objects)
{
	ECS source;
	Object first;
	Object second;
	source.add_object(first);
	source.add_object(second);
	source.spawn_tileset(0, 0, 2.5f, "board");
	source.move_to_tile({ 1, 2 }, first.get_id(), "board");
	source.move_to_tile({ 3, 4 }, second.get_id(), "board");
	Serializer serializer;
	source.TileSystem::serialize(serializer);

	ECS restored;
	restored.TileSystem::deserialize(Deserializer::parse(serializer.emit()));
	EXPECT_EQ(restored.get_tile_coord(first.get_id(), "board"), TileCoord(1, 2));
	EXPECT_EQ(restored.get_tile_coord(second.get_id(), "board"), TileCoord(3, 4));
	ASSERT_NE(restored.get_tile({ 1, 2 }, "board"), nullptr);
	EXPECT_EQ(restored.get_tile({ 1, 2 }, "board")->get_objects()[0], first.get_id());
}

TEST(TileSystemSerialization, duplicate_tilesets_fail_atomically)
{
	ECS ecs;
	Object existing;
	ecs.add_object(existing);
	ecs.spawn_tileset(0, 0, 3.0f, "existing");
	ecs.move_to_tile({ 0, 0 }, existing.get_id(), "existing");
	const auto malformed = Deserializer::parse(
		"tile_system:\n"
		"  previous_hovered: ~\n"
		"  tilesets:\n"
		"    - {tileset_id: duplicate, cell_size: 1, tiles: [], tile_objects: []}\n"
		"    - {tileset_id: duplicate, cell_size: 1, tiles: [], tile_objects: []}\n");
	EXPECT_THROW(ecs.TileSystem::deserialize(malformed), SerializationError);
	EXPECT_NE(ecs.get_tile({ 0, 0 }, "existing"), nullptr);
}
