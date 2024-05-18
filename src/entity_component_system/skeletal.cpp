#include "skeletal.hpp"
#include "ecs.hpp"

#include <stdexcept>


std::vector<SDS::Bone> SkeletalComponent::get_bones_data() const
{
	// TODO: can potentially rely on SkeletalSystem::process to do this...
	// This is kinda expensive

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

SkeletonID SkeletalSystem::add_skeleton(const std::vector<Bone>& bones)
{
	const auto id = SkeletonID::generate_new_id();
	skeletons.emplace(id, bones);
	return id;
}

void SkeletalSystem::remove_entity(Entity id)
{
	for (const auto& renderable : get_ecs().get_object(id).renderables)
	{
		if (renderable.skeleton_id)
		{
			skeletons.erase(*renderable.skeleton_id);
		}
	}
}

// void SkeletalSystem::add_bone_visualisers(Entity id, const std::vector<Entity>& bones)
// {
// 	if (skeletons.find(id) == skeletons.end())
// 	{
// 		throw std::runtime_error("SkeletalSystem::add_bone_visualisers: id not found");
// 	}

// 	skeletons[id].set_visualisers(bones);
// }

void SkeletalAnimationSystem::process(const float delta_secs)
{
	for (const auto& [skeleton_id, animation_id] : active_animations)
	{
		AnimationState& state = animation_states[skeleton_id];
		SkeletalAnimation& animation = animations[animation_id];
		auto& bones = get_ecs().get_skeletal_component(skeleton_id).get_bones();

		state.current_animation_elapsed_secs += delta_secs;

		bool still_animating = false;
		for (int bone_idx = 0; bone_idx < bones.size(); ++bone_idx)
		{
			still_animating |= animation.bone_animations[bone_idx].get_transform(
				state.current_animation_elapsed_secs, 
				bones[bone_idx].relative_transform);
		}

		if (!still_animating)
		{
			if (state.should_loop)
			{
				state.current_animation_elapsed_secs = 0;
			} else 
			{
				for (auto& bone : bones)
				{
					bone.relative_transform = bone.original_transform;
				}
				active_animations.erase(skeleton_id);
				animation_states.erase(skeleton_id);
			}
		}
	}
}

AnimationID SkeletalAnimationSystem::add_skeletal_animation(const std::string& name,
                                                     		std::vector<BoneAnimation>&& bone_animations)
{
	SkeletalAnimation animation;
	animation.name = name;
	animation.bone_animations = std::move(bone_animations);
	const auto id = AnimationID::generate_new_id();
	animations.emplace(id, std::move(animation));

	return id;
}

void SkeletalAnimationSystem::play_animation(SkeletonID skeleton_id, 
											 AnimationID animation_id,
											 bool loop) 
{
	active_animations.emplace(skeleton_id, animation_id);
	AnimationState state;
	state.should_loop = loop;
	state.current_animation_elapsed_secs = 0.0f;
	animation_states[skeleton_id] = state;
}

void SkeletalAnimationSystem::remove_entity(Entity id) 
{
	for (const auto& renderable : get_ecs().get_object(id).renderables)
	{
		if (renderable.skeleton_id)
		{
			active_animations.erase(*renderable.skeleton_id);
			animation_states.erase(*renderable.skeleton_id);
		}
	}
}
