#include "skeletal.hpp"
#include "ecs.hpp"

#include <stdexcept>
#include <ranges>
#include <algorithm>
#include <functional>
#include <unordered_map>


namespace
{
template<typename T>
T cubic_spline(
	const BoneAnimation::TrackKey<T>& first,
	const BoneAnimation::TrackKey<T>& second,
	const float t)
{
	const float duration = second.animation_stage_secs - first.animation_stage_secs;
	const float t2 = t * t;
	const float t3 = t2 * t;
	return (2.0f * t3 - 3.0f * t2 + 1.0f) * first.value
		+ (t3 - 2.0f * t2 + t) * duration * first.out_tangent
		+ (-2.0f * t3 + 3.0f * t2) * second.value
		+ (t3 - t2) * duration * second.in_tangent;
}

template<typename T, typename LinearInterpolator>
T evaluate_track(
	const BoneAnimation::Track<T>& track,
	const float animation_stage_secs,
	LinearInterpolator linear_interpolator)
{
	if (track.keys.size() == 1 || animation_stage_secs <= track.keys.front().animation_stage_secs)
		return track.keys.front().value;
	if (animation_stage_secs >= track.keys.back().animation_stage_secs)
		return track.keys.back().value;

	auto second = std::upper_bound(
		track.keys.begin(), track.keys.end(), animation_stage_secs,
		[](const float time, const auto& key){ return time < key.animation_stage_secs; });
	const auto& first_key = *(second - 1);
	const auto& second_key = *second;
	if (track.interpolation == BoneAnimation::Interpolation::STEP)
		return first_key.value;

	const float duration = second_key.animation_stage_secs - first_key.animation_stage_secs;
	const float t = (animation_stage_secs - first_key.animation_stage_secs) / duration;
	if (track.interpolation == BoneAnimation::Interpolation::CUBIC_SPLINE)
		return cubic_spline(first_key, second_key, t);
	return linear_interpolator(first_key.value, second_key.value, t);
}
}


SkeletalRigSignature make_skeletal_rig_signature(const std::vector<Bone>& bones)
{
	std::unordered_map<std::string, size_t> name_counts;
	for (const auto& bone : bones)
		if (!bone.name.empty())
			++name_counts[bone.name];

	std::vector<std::string> labels(bones.size());
	for (size_t index = 0; index < bones.size(); ++index)
	{
		const auto& name = bones[index].name;
		labels[index] = !name.empty() && name_counts[name] == 1
			? name : "#" + std::to_string(index);
	}

	SkeletalRigSignature signature;
	signature.reserve(bones.size());
	for (size_t index = 0; index < bones.size(); ++index)
	{
		const auto parent = bones[index].parent_node;
		if (parent != Bone::NO_PARENT && parent >= bones.size())
			throw std::runtime_error("Skeletal rig contains an invalid parent index");
		signature.push_back({ labels[index], parent == Bone::NO_PARENT ? std::string{} : labels[parent] });
	}
	std::ranges::sort(signature, {}, &SkeletalRigBone::name);
	return signature;
}


std::vector<SDS::Bone> SkeletalComponent::get_bones_data() const
{
	std::vector<SDS::Bone> final_bones_data(bones.size());
	std::vector<bool> resolved(bones.size(), false);
	std::function<void(uint32_t)> resolve = [&](const uint32_t index)
	{
		if (resolved[index])
			return;
		const auto& bone = bones[index];
		if (bone.parent_node != Bone::NO_PARENT)
		{
			if (bone.parent_node >= bones.size())
				throw std::runtime_error("SkeletalComponent: invalid bone parent index");
			resolve(bone.parent_node);
			final_bones_data[index].final_transform =
				final_bones_data[bone.parent_node].final_transform * bone.relative_transform.get_mat4();
		}
		else
		{
			final_bones_data[index].final_transform = bone.relative_transform.get_mat4();
		}
		final_bones_data[index].inverse_transform = bone.inverse_bind_pose.get_mat4();
		resolved[index] = true;
	};
	for (uint32_t i = 0; i < bones.size(); ++i)
		resolve(i);

	for (uint32_t i = 0; i < bones.size(); i++)
	{
		final_bones_data[i].final_transform *= bones[i].inverse_bind_pose.get_mat4();
	}

	return final_bones_data;
}

bool BoneAnimation::get_transform(const float animation_stage_secs, Maths::Transform& out_transform) const
{
	const bool has_tracks = !translation_track.keys.empty() || !rotation_track.keys.empty() || !scale_track.keys.empty();
	if (has_tracks)
	{
		if (animation_stage_secs > animation_end_secs)
			return false;

		out_transform = base_transform;
		if (!translation_track.keys.empty())
		{
			out_transform.set_pos(evaluate_track(translation_track, animation_stage_secs,
				[](const glm::vec3& a, const glm::vec3& b, const float t){ return glm::mix(a, b, t); }));
		}
		if (!rotation_track.keys.empty())
		{
			const glm::vec4 value = evaluate_track(rotation_track, animation_stage_secs,
				[](const glm::vec4& a, const glm::vec4& b, const float t)
				{
					const glm::quat first(a.w, a.x, a.y, a.z);
					const glm::quat second(b.w, b.x, b.y, b.z);
					const glm::quat result = glm::slerp(first, second, t);
					return glm::vec4(result.x, result.y, result.z, result.w);
				});
			out_transform.set_orient(glm::normalize(glm::quat(value.w, value.x, value.y, value.z)));
		}
		if (!scale_track.keys.empty())
		{
			out_transform.set_scale(evaluate_track(scale_track, animation_stage_secs,
				[](const glm::vec3& a, const glm::vec3& b, const float t){ return glm::mix(a, b, t); }));
		}
		return true;
	}

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
	if (const auto skeleton_id = get_ecs().get_object(id).get_skeleton_id())
	{
		skeletons.erase(*skeleton_id);
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
	std::vector<SkeletonID> skeletons_to_remove;
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
				skeletons_to_remove.push_back(skeleton_id);
			}
		}
	}

	std::ranges::for_each(skeletons_to_remove, [this](SkeletonID id) 
	{ 
		active_animations.erase(id);
		animation_states.erase(id);
	});
}

AnimationID SkeletalAnimationSystem::add_skeletal_animation(
	const std::string& name,
	std::vector<BoneAnimation>&& bone_animations,
	SkeletalRigSignature rig_signature,
	std::string source)
{
	SkeletalAnimation animation;
	animation.name = name;
	animation.source = std::move(source);
	animation.rig_signature = std::move(rig_signature);
	animation.bone_animations = std::move(bone_animations);
	const auto id = AnimationID::generate_new_id();
	animations.emplace(id, std::move(animation));

	return id;
}

bool SkeletalAnimationSystem::is_animation_compatible(
	const SkeletonID skeleton_id,
	const AnimationID animation_id) const
{
	const auto& bones = get_ecs().get_skeletal_component(skeleton_id).get_bones();
	const auto& animation = animations.at(animation_id);
	return animation.bone_animations.size() == bones.size()
		&& animation.rig_signature == make_skeletal_rig_signature(bones);
}

void SkeletalAnimationSystem::play_animation(SkeletonID skeleton_id, 
											 AnimationID animation_id,
											 bool loop) 
{
	if (!is_animation_compatible(skeleton_id, animation_id))
		throw std::runtime_error("SkeletalAnimationSystem: animation is incompatible with the target skeleton");
	for (auto& bone : get_ecs().get_skeletal_component(skeleton_id).get_bones())
		bone.relative_transform = bone.original_transform;
	active_animations.insert_or_assign(skeleton_id, animation_id);
	AnimationState state;
	state.should_loop = loop;
	state.current_animation_elapsed_secs = 0.0f;
	animation_states[skeleton_id] = state;
}

void SkeletalAnimationSystem::remove_entity(Entity id) 
{
	if (const auto skeleton_id = get_ecs().get_object(id).get_skeleton_id())
	{
		active_animations.erase(*skeleton_id);
		animation_states.erase(*skeleton_id);
	}
}
