#include "animation.hpp"
#include "ecs.hpp"
#include "serialization/serialization_helpers.hpp"

#include <algorithm>
#include <stdexcept>


void AnimationSystem::process(const float delta_secs) 
{
	std::vector<ObjectID> to_remove;

	for (auto& [id, animation_sequence] : entities)
	{
		animation_sequence.elapsed_secs += delta_secs;
		const float t = std::min(animation_sequence.elapsed_secs / animation_sequence.duration_secs, 1.0f);
		const auto pos = glm::mix(animation_sequence.initial_transform.get_pos(), animation_sequence.final_transform.get_pos(), t);
		const auto scale = glm::mix(animation_sequence.initial_transform.get_scale(), animation_sequence.final_transform.get_scale(), t);
		const auto quat = glm::slerp(animation_sequence.initial_transform.get_orient(), animation_sequence.final_transform.get_orient(), t);
		Object& object = get_ecs().get_object(id);
		if (animation_sequence.is_relative)
		{
			object.set_relative_position(pos);
			object.set_relative_scale(scale);
			object.set_relative_rotation(quat);
		} else
		{
			object.set_position(pos);
			object.set_scale(scale);
			object.set_rotation(quat);
		}

		if (t >= 1.0f)
		{
			to_remove.push_back(id);
		}
	}

	for (const auto& id : to_remove)
	{
		entities.erase(id);
	}
}

void AnimationSystem::add_entity(const ObjectID id, const AnimationSequence& sequence)
{
	if (!entities.emplace(id, sequence).second)
	{
		throw std::runtime_error("AnimationSystem::add_component: component already exists");
	}
}

void AnimationSystem::serialize(Serializer& out) const
{
	std::vector<ObjectID> ids;
	ids.reserve(entities.size());
	for (const auto& [id, _] : entities)
		ids.push_back(id);
	std::ranges::sort(ids);
	auto entries = out.sequence("animation_system");
	for (const auto id : ids) {
		const auto& sequence = entities.at(id);
		auto entry = entries.append_map();
		entry.write("entity_id", id.get_underlying());
		Serialization::write_transform(entry, "initial_transform", sequence.initial_transform);
		Serialization::write_transform(entry, "final_transform", sequence.final_transform);
		entry.write("duration_secs", sequence.duration_secs);
		entry.write("is_relative", sequence.is_relative);
		entry.write("elapsed_secs", sequence.elapsed_secs);
	}
}

void AnimationSystem::deserialize(const Deserializer& in)
{
	std::unordered_map<ObjectID, AnimationSequence> restored;
	const auto entries = in.child("animation_system").elements();
	for (std::size_t index = 0; index < entries.size(); ++index) {
		const auto& entry = entries[index];
		const ObjectID id(entry.read<std::uint64_t>("entity_id"));
		AnimationSequence sequence(
			Serialization::read_transform(entry, "initial_transform"),
			Serialization::read_transform(entry, "final_transform"),
			entry.read<float>("duration_secs"), entry.read<bool>("is_relative"));
		sequence.elapsed_secs = entry.read<float>("elapsed_secs");
		if (!restored.emplace(id, std::move(sequence)).second) {
			throw SerializationError("Duplicate animated entity at $.animation_system["
				+ std::to_string(index) + "].entity_id");
		}
	}
	entities = std::move(restored);
}
