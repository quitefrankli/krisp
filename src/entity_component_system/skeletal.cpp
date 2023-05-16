#include "skeletal.hpp"
#include "ecs.hpp"

#include <stdexcept>


std::vector<SDS::Bone> SkeletalComponent::get_bones_data() const
{
	// TODO: can potentially rely on SkeletalSystem::process to do this...

	std::vector<SDS::Bone> final_bones_data(bones.size());
	final_bones_data[0].final_transform = bones[0].relative_transform.get_mat4();
	final_bones_data[0].inverse_transform = bones[0].inverse_bind_pose.get_mat4();
	for (uint32_t i = 1; i < bones.size(); ++i)
	{
		const auto& bone = bones[i];
		final_bones_data[i].final_transform = 
			final_bones_data[bone.parent_node].final_transform * bone.relative_transform.get_mat4();
		final_bones_data[i].inverse_transform = bone.inverse_bind_pose.get_mat4();
	}

	return final_bones_data;
}

void SkeletalComponent::set_visualisers(const std::vector<Entity>& visualisers) 
{
	if (visualisers.size() != bones.size())
	{
		throw std::runtime_error("SkeletalComponent::set_visualisers: visualisers.size() != bones.size()");
	}
	this->visualisers = visualisers;
}

std::vector<Entity> SkeletalSystem::get_all_skinned_entities() const
{
	std::vector<Entity> skinned_entities;
	for (const auto& [entity, skeleton] : skeletons)
	{
		skinned_entities.push_back(entity);
	}

	return skinned_entities;
}

void SkeletalSystem::add_bone_visualisers(Entity id, const std::vector<Entity>& bones)
{
	if (skeletons.find(id) == skeletons.end())
	{
		throw std::runtime_error("SkeletalSystem::add_bone_visualisers: id not found");
	}

	skeletons[id].set_visualisers(bones);
}

void SkeletalSystem::process(const float delta_secs) 
{
	// update all the bone visualisers
	for (auto& [entity, skeleton] : skeletons)
	{
		if (skeleton.get_visualisers().empty())
		{
			continue;
		}

		const auto& bones = skeleton.get_bones();
		std::vector<Object*> visualisers;
		for (const auto visualiser : skeleton.get_visualisers())
		{
			visualisers.push_back(&get_ecs().get_object(visualiser));
		}

		visualisers[0]->set_transform(bones[0].relative_transform.get_mat4());
		for (int i = 1; i < visualisers.size(); ++i)
		{
			const auto& bone = bones[i];
			auto* visualiser = visualisers[i];

			visualiser->set_transform(visualisers[bone.parent_node]->get_transform() * bone.relative_transform.get_mat4());
		}
	}
}
