#include "physics.hpp"
#include "entity_component_system/ecs.hpp"
#include "serialization/serialization_helpers.hpp"

#include <algorithm>

namespace
{
std::string gravity_type_name(const GravitySystem::GravityType type, const std::size_t index)
{
	switch (type) {
	case GravitySystem::GravityType::DISABLED:
		return "disabled";
	case GravitySystem::GravityType::EARTH_LIKE:
		return "earth_like";
	case GravitySystem::GravityType::TRUE:
		return "true";
	}
	throw SerializationError("Unsupported gravity type at $.physics_system.force_systems["
		+ std::to_string(index) + "].gravity_type");
}

GravitySystem::GravityType parse_gravity_type(const std::string& type, const std::size_t index)
{
	if (type == "disabled")
		return GravitySystem::GravityType::DISABLED;
	if (type == "earth_like")
		return GravitySystem::GravityType::EARTH_LIKE;
	if (type == "true")
		return GravitySystem::GravityType::TRUE;
	throw SerializationError("Unsupported gravity type at $.physics_system.force_systems["
		+ std::to_string(index) + "].gravity_type");
}
}


PhysicsSystem::PhysicsSystem()
{
	force_systems.push_back(std::make_unique<GravitySystem>());
}

void PhysicsSystem::process(const float delta_secs)
{
	auto& ecs = get_ecs();
	prepare_components();

	//
	// With Verlet Integration
	//

	//
	// 1. advance position
	//
	for (auto& [id, physics_comp] : physics_entities)
	{
		auto& object = ecs.get_object(id);
		auto pos = object.get_position();
		pos += physics_comp.velocity * delta_secs + 0.5f * physics_comp.acceleration * delta_secs * delta_secs;
		object.set_position(pos);
	}

	//
	// 2. recompute forces at new position
	//
	for (auto& force_system : force_systems)
	{
		force_system->compute_forces(delta_secs, physics_entities);
	}

	//
	// 3. apply forces and update velocity
	//
	for (auto& [id, physics_comp] : physics_entities)
	{
		auto& object = ecs.get_object(id);
		const glm::vec3 new_acceleration = physics_comp._net_force / physics_comp.mass;
		physics_comp.velocity += 0.5f * (physics_comp.acceleration + new_acceleration) * delta_secs;
		physics_comp.acceleration = new_acceleration;
	}
}

GravitySystem& PhysicsSystem::get_gravity_system()
{
	return static_cast<GravitySystem&>(*force_systems[0]);
}

const GravitySystem& PhysicsSystem::get_gravity_system() const
{
	return static_cast<const GravitySystem&>(*force_systems[0]);
}

void PhysicsSystem::serialize(Serializer& out) const
{
	auto system = out.map("physics_system");
	auto components_out = system.sequence("components");
	std::vector<ObjectID> ids;
	ids.reserve(physics_entities.size());
	for (const auto& [id, _] : physics_entities)
		ids.push_back(id);
	std::ranges::sort(ids);
	for (const auto id : ids) {
		const auto& component = physics_entities.at(id);
		auto entry = components_out.append_map();
		entry.write("entity_id", id.get_underlying());
		entry.write("mass", component.mass);
		EcsSerialization::write_vec3(entry, "position", component.position);
		EcsSerialization::write_vec3(entry, "velocity", component.velocity);
		EcsSerialization::write_quat(entry, "angular_velocity", component.angular_velocity);
		EcsSerialization::write_vec3(entry, "acceleration", component.acceleration);
		EcsSerialization::write_vec3(entry, "net_force", component._net_force);
	}

	auto forces_out = system.sequence("force_systems");
	for (std::size_t index = 0; index < force_systems.size(); ++index) {
		const auto* gravity = dynamic_cast<const GravitySystem*>(force_systems[index].get());
		if (!gravity) {
			throw SerializationError("Unsupported force system at $.physics_system.force_systems["
				+ std::to_string(index) + "].type");
		}
		auto entry = forces_out.append_map();
		entry.write("type", "gravity");
		entry.write("gravity_type", gravity_type_name(gravity->get_gravity_type(), index));
	}
}

void PhysicsSystem::deserialize(const Deserializer& in)
{
	const auto system = in.child("physics_system");
	std::unordered_map<ObjectID, PhysicsComponent> restored_components;
	const auto component_entries = system.child("components").elements();
	for (std::size_t index = 0; index < component_entries.size(); ++index) {
		const auto& entry = component_entries[index];
		const ObjectID id(entry.read<std::uint64_t>("entity_id"));
		PhysicsComponent component;
		component.mass = entry.read<float>("mass");
		component.position = EcsSerialization::read_vec3(entry, "position");
		component.velocity = EcsSerialization::read_vec3(entry, "velocity");
		component.angular_velocity = EcsSerialization::read_quat(entry, "angular_velocity");
		component.acceleration = EcsSerialization::read_vec3(entry, "acceleration");
		component._net_force = EcsSerialization::read_vec3(entry, "net_force");
		if (!restored_components.emplace(id, component).second) {
			throw SerializationError("Duplicate physics entity at $.physics_system.components["
				+ std::to_string(index) + "].entity_id");
		}
	}

	std::vector<std::unique_ptr<ForceSystem>> restored_forces;
	const auto force_entries = system.child("force_systems").elements();
	if (force_entries.empty())
		throw SerializationError("Physics force systems cannot be empty at $.physics_system.force_systems");
	for (std::size_t index = 0; index < force_entries.size(); ++index) {
		const auto& entry = force_entries[index];
		const auto type = entry.read<std::string>("type");
		if (type != "gravity") {
			throw SerializationError("Unsupported force system at $.physics_system.force_systems["
				+ std::to_string(index) + "].type");
		}
		auto gravity = std::make_unique<GravitySystem>();
		gravity->set_gravity_type(parse_gravity_type(entry.read<std::string>("gravity_type"), index));
		restored_forces.push_back(std::move(gravity));
	}

	physics_entities = std::move(restored_components);
	force_systems = std::move(restored_forces);
}

void PhysicsSystem::prepare_components()
{
	auto& ecs = get_ecs();
	for (auto& [id, physics_comp] : physics_entities)
	{
		physics_comp.position = ecs.get_object(id).get_position();
		physics_comp._net_force = Maths::zero_vec;
	}
}
