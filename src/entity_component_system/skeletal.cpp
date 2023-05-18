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

	for (uint32_t i = 0; i < bones.size(); i++)
	{
		final_bones_data[i].final_transform *= bones[i].inverse_bind_pose.get_mat4();
	}

	return final_bones_data;
}

bool BoneAnimation::get_transform(const float animation_stage_secs, Maths::Transform& out_transform) const
{
	if (key_frames.empty())
	{
		return false;
	}

	if (key_frames.size() == 1) // shouldn't really be possible
	{
		out_transform = key_frames[0].transform;
		return true;
	}

	if (animation_stage_secs < animation_start_secs)
	{
		return true;
	}
	
	if (animation_stage_secs > animation_end_secs)
	{
		return false;
	}

	// find the key frames that the time is between
	// As an optimisation, we can use binary search for animations with many key frames
	uint32_t key_frame_index = 0;
	for (; key_frame_index < key_frames.size() - 1; ++key_frame_index)
	{
		if (animation_stage_secs >= key_frames[key_frame_index].animation_stage_secs &&
			animation_stage_secs < key_frames[key_frame_index + 1].animation_stage_secs)
		{
			break;
		}
	}

	const auto& key_frame_0 = key_frames[key_frame_index];
	const auto& key_frame_1 = key_frames[key_frame_index + 1];
	const float time_between_frames = key_frame_1.animation_stage_secs - key_frame_0.animation_stage_secs;
	const float blend_factor = (animation_stage_secs - key_frame_0.animation_stage_secs) / time_between_frames;

	out_transform.set_pos(glm::mix(key_frame_0.transform.get_pos(), key_frame_1.transform.get_pos(), blend_factor));
	out_transform.set_orient(glm::slerp(key_frame_0.transform.get_orient(), key_frame_1.transform.get_orient(), blend_factor));
	out_transform.set_scale(glm::mix(key_frame_0.transform.get_scale(), key_frame_1.transform.get_scale(), blend_factor));

	return true;
}

void SkeletalComponent::set_visualisers(const std::vector<Entity>& visualisers) 
{
	if (visualisers.size() != bones.size())
	{
		throw std::runtime_error("SkeletalComponent::set_visualisers: visualisers.size() != bones.size()");
	}
	this->visualisers = visualisers;
}

void SkeletalComponent::set_animation(const std::string& animation_name) 
{
	if (animations.find(animation_name) == animations.end())
	{
		throw std::runtime_error("SkeletalComponent::set_animation: animation_name not found");
	}

	current_animation = animation_name;
	current_animation_elapsed_secs = 0.0f;
}

void SkeletalComponent::animate(const float delta_secs) 
{
	if (current_animation.empty())
	{
		return;
	}

	auto& animation = animations[current_animation];
	current_animation_elapsed_secs += delta_secs;

	bool still_animating = false;
	for (int bone_idx = 0; bone_idx < bones.size(); ++bone_idx)
	{
		still_animating |= animation[bone_idx].get_transform(
			current_animation_elapsed_secs, 
			bones[bone_idx].relative_transform);
	}

	if (!still_animating)
	{
		if (should_loop_animation)
		{
			current_animation_elapsed_secs = 0;
		} else 
		{
			current_animation.clear();
			for (auto& bone : bones)
			{
				bone.relative_transform = bone.original_transform;
			}
		}
	}
}

std::vector<std::string> SkeletalComponent::get_animations() const
{
	std::vector<std::string> animation_names;
	for (const auto& [name, _] : animations)
	{
		animation_names.push_back(name);
	}

	return animation_names;
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

void SkeletalSystem::animate_skeleton(Entity id, const std::string& animation_name) 
{
	if (skeletons.find(id) == skeletons.end())
	{
		throw std::runtime_error("SkeletalSystem::animate_skeleton: id not found");
	}

	skeletons[id].set_animation(animation_name);
}

std::vector<std::string> SkeletalSystem::get_skeletal_animations(Entity id) const
{
	if (skeletons.find(id) == skeletons.end())
	{
		throw std::runtime_error("SkeletalSystem::get_skeletal_animations: id not found");
	}

	return skeletons.at(id).get_animations();
}

void SkeletalSystem::process(const float delta_secs)
{
	for (auto& [entity, skeleton] : skeletons)
	{
		// update all animations
		skeleton.animate(delta_secs);

		// update all the bone visualisers
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
