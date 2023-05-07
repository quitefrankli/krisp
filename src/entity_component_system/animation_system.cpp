#include "animation_system.hpp"
#include "ecs_manager.hpp"

#include <stdexcept>


AnimationECS::AnimationECS(ECSManager& manager) :
	ECSComponent(manager)
{
}

void AnimationECS::process(const float delta_secs) 
{
	std::vector<ObjectID> to_remove;

	for (auto& [id, animation_sequence] : entities)
	{
		animation_sequence.elapsed_secs += delta_secs;
		const float t = std::min(animation_sequence.elapsed_secs / animation_sequence.duration_secs, 1.0f);
		const auto pos = glm::mix(animation_sequence.initial_transform.get_pos(), animation_sequence.final_transform.get_pos(), t);
		const auto scale = glm::mix(animation_sequence.initial_transform.get_scale(), animation_sequence.final_transform.get_scale(), t);
		const auto quat = glm::slerp(animation_sequence.initial_transform.get_orient(), animation_sequence.final_transform.get_orient(), t);
		Object& object = ecs_manager.get_object(id);
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

void AnimationECS::add_entity(const ObjectID id, const AnimationSequence& sequence)
{
	if (!entities.emplace(id, sequence).second)
	{
		throw std::runtime_error("AnimationECS::add_component: component already exists");
	}
}
