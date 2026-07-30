#include "skeletal.hpp"
#include "ecs.hpp"
#include "serialization/resource_provenance.hpp"

#include <stdexcept>
#include <ranges>
#include <algorithm>
#include <cmath>
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

glm::quat evaluate_rotation_track(
	const BoneAnimation::Track<glm::vec4>& track,
	const float animation_stage_secs)
{
	const auto to_quaternion = [](const glm::vec4& value)
	{
		return glm::normalize(glm::quat(value.w, value.x, value.y, value.z));
	};
	if (track.keys.size() == 1 || animation_stage_secs <= track.keys.front().animation_stage_secs)
		return to_quaternion(track.keys.front().value);
	if (animation_stage_secs >= track.keys.back().animation_stage_secs)
		return to_quaternion(track.keys.back().value);

	const auto second = std::upper_bound(
		track.keys.begin(), track.keys.end(), animation_stage_secs,
		[](const float time, const auto& key){ return time < key.animation_stage_secs; });
	const auto& first_key = *(second - 1);
	const auto& second_key = *second;
	if (track.interpolation == BoneAnimation::Interpolation::STEP)
		return to_quaternion(first_key.value);

	const float duration = second_key.animation_stage_secs - first_key.animation_stage_secs;
	const float t = (animation_stage_secs - first_key.animation_stage_secs) / duration;
	// Cubic quaternion tangents can introduce visible normalized-Hermite overshoot.
	// Use shortest-path spherical interpolation for smooth rotation tracks.
	return glm::slerp(to_quaternion(first_key.value), to_quaternion(second_key.value), t);
}

float animation_duration(const SkeletalAnimation& animation)
{
	float duration_secs = 0.0f;
	for (const auto& bone_animation : animation.bone_animations)
		duration_secs = std::max(duration_secs, bone_animation.animation_end_secs);
	return duration_secs;
}

void align_rotation_track_hemisphere(BoneAnimation::Track<glm::vec4>& track)
{
	for (size_t index = 1; index < track.keys.size(); ++index)
	{
		auto& previous = track.keys[index - 1];
		auto& current = track.keys[index];
		if (glm::dot(previous.value, current.value) >= 0.0f)
			continue;
		current.value = -current.value;
		current.in_tangent = -current.in_tangent;
		current.out_tangent = -current.out_tangent;
	}
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

std::vector<glm::mat4> SkeletalComponent::get_model_space_bone_transforms() const
{
	std::vector<glm::mat4> transforms(bones.size());
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
			transforms[index] = transforms[bone.parent_node] * bone.relative_transform.get_mat4();
		}
		else
			transforms[index] = bone.relative_transform.get_mat4();
		resolved[index] = true;
	};
	for (uint32_t i = 0; i < bones.size(); ++i)
		resolve(i);
	return transforms;
}

SkeletalRenderStateSnapshot SkeletalComponent::snapshot_render_state() const
{
	SkeletalRenderStateSnapshot snapshot;
	snapshot.parent_indices.reserve(bones.size());
	snapshot.inverse_bind_poses.reserve(bones.size());
	snapshot.local_transforms.reserve(bones.size());
	for (const auto& bone : bones)
	{
		snapshot.parent_indices.push_back(bone.parent_node);
		snapshot.inverse_bind_poses.push_back(bone.inverse_bind_pose.get_mat4());
		snapshot.local_transforms.push_back(bone.relative_transform.get_mat4());
	}
	return snapshot;
}

bool BoneAnimation::get_transform(const float animation_stage_secs, Maths::Transform& out_transform) const
{
	if (translation_track.keys.empty() && rotation_track.keys.empty() && scale_track.keys.empty())
		return false;
	if (animation_stage_secs > animation_end_secs)
		return false;

	out_transform = base_transform;
	if (!translation_track.keys.empty())
		out_transform.set_pos(evaluate_track(translation_track, animation_stage_secs,
			[](const glm::vec3& a, const glm::vec3& b, const float t){ return glm::mix(a, b, t); }));
	if (!rotation_track.keys.empty())
		out_transform.set_orient(evaluate_rotation_track(rotation_track, animation_stage_secs));
	if (!scale_track.keys.empty())
		out_transform.set_scale(evaluate_track(scale_track, animation_stage_secs,
			[](const glm::vec3& a, const glm::vec3& b, const float t){ return glm::mix(a, b, t); }));
	return true;
}

SkeletonID SkeletalSystem::add_skeleton(const std::vector<Bone>& bones)
{
	const auto id = SkeletonID::generate_new_id();
	skeletons.emplace(id, bones);
	return id;
}

void SkeletalSystem::attach_skeleton(const Entity id, const SkeletonID skeleton_id)
{
	if (!skeletons.contains(skeleton_id))
		throw std::out_of_range("SkeletalSystem::attach_skeleton: skeleton not found");
	entity_skeletons.insert_or_assign(id, skeleton_id);
}

std::optional<SkeletonID> SkeletalSystem::get_skeleton_id(const Entity id) const
{
	const auto it = entity_skeletons.find(id);
	return it == entity_skeletons.end() ? std::nullopt : std::optional<SkeletonID>(it->second);
}

std::vector<SkeletonID> SkeletalSystem::get_skeleton_ids() const
{
	std::vector<SkeletonID> ids;
	ids.reserve(skeletons.size());
	for (const auto& [id, _] : skeletons)
		ids.push_back(id);
	return ids;
}

bool SkeletalSystem::attach_entity_to_bone(
	const Entity attached,
	const Entity skeleton_entity,
	const std::string_view bone_name,
	Maths::Transform local_transform)
{
	const auto skeleton = get_skeleton_id(skeleton_entity);
	if (!skeleton || attached == skeleton_entity)
		return false;
	const auto& bones = get_skeletal_component(*skeleton).get_bones();
	const auto bone = std::ranges::find(bones, bone_name, &Bone::name);
	if (bone == bones.end())
		return false;
	bone_attachments.insert_or_assign(attached, BoneAttachment{
		.skeleton_entity = skeleton_entity,
		.bone_index = static_cast<uint32_t>(std::distance(bones.begin(), bone)),
		.local_transform = std::move(local_transform),
	});
	return true;
}

bool SkeletalSystem::detach_entity_from_bone(const Entity attached)
{
	return bone_attachments.erase(attached) > 0;
}

void SkeletalSystem::process(const float)
{
	std::unordered_map<Entity, std::vector<glm::mat4>> model_space_poses;
	for (const auto& [attached, attachment] : bone_attachments)
	{
		const auto skeleton = get_skeleton_id(attachment.skeleton_entity);
		if (!skeleton)
			continue;
		auto [pose, inserted] = model_space_poses.try_emplace(attachment.skeleton_entity);
		if (inserted)
			pose->second = get_skeletal_component(*skeleton).get_model_space_bone_transforms();
		if (attachment.bone_index >= pose->second.size())
			continue;

		const Object& skeleton_object = get_ecs().get_object(attachment.skeleton_entity);
		glm::mat4 visual_transform = Maths::identity_mat;
		if (!skeleton_object.renderables.empty())
			visual_transform = skeleton_object.renderables.front().local_transform.get_mat4();
		get_ecs().get_object(attached).set_transform(
			skeleton_object.get_transform() *
			visual_transform *
			pose->second[attachment.bone_index] *
			attachment.local_transform.get_mat4());
	}
}

void SkeletalSystem::remove_entity(Entity id)
{
	bone_attachments.erase(id);
	std::erase_if(bone_attachments, [id](const auto& entry)
	{
		return entry.second.skeleton_entity == id;
	});
	const auto it = entity_skeletons.find(id);
	if (it == entity_skeletons.end())
		return;
	skeletons.erase(it->second);
	entity_skeletons.erase(it);
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
		if (state.paused)
			continue;
		state.current_animation_elapsed_secs += delta_secs * state.playback_speed;
		const float duration_secs = animation_duration(animations.at(animation_id));
		if (state.should_loop && duration_secs > 0.0f)
		{
			state.current_animation_elapsed_secs = std::fmod(
				state.current_animation_elapsed_secs, duration_secs);
			if (state.current_animation_elapsed_secs < 0.0f)
				state.current_animation_elapsed_secs += duration_secs;
		}
		else if (state.current_animation_elapsed_secs > duration_secs)
		{
			auto& component = get_ecs().get_skeletal_component(skeleton_id);
			for (auto& bone : component.get_bones())
				bone.relative_transform = bone.original_transform;
			skeletons_to_remove.push_back(skeleton_id);
			continue;
		}

		if (state.fade)
		{
			state.fade->elapsed_secs += std::abs(delta_secs * state.playback_speed);
		}
		apply_animation_pose(skeleton_id);
		if (state.fade && state.fade->elapsed_secs >= state.fade->duration_secs)
			state.fade.reset();
	}

	std::ranges::for_each(skeletons_to_remove, [this](SkeletonID id) 
	{ 
		active_animations.erase(id);
		animation_states.erase(id);
	});
}

void SkeletalAnimationSystem::apply_animation_pose(const SkeletonID skeleton_id)
{
	const auto animation_id = active_animations.at(skeleton_id);
	const auto& animation = animations.at(animation_id);
	auto& state = animation_states.at(skeleton_id);
	auto& component = get_ecs().get_skeletal_component(skeleton_id);
	auto& bones = component.get_bones();
	std::vector<Maths::Transform> target_pose;
	target_pose.reserve(bones.size());
	for (const auto& bone : bones)
		target_pose.push_back(bone.original_transform);
	for (size_t bone_idx = 0; bone_idx < bones.size(); ++bone_idx)
		animation.bone_animations[bone_idx].get_transform(
			state.current_animation_elapsed_secs, target_pose[bone_idx]);

	if (!state.fade)
	{
		for (size_t bone_idx = 0; bone_idx < bones.size(); ++bone_idx)
			bones[bone_idx].relative_transform = target_pose[bone_idx];
		return;
	}

	const float blend = std::clamp(
		state.fade->elapsed_secs / state.fade->duration_secs, 0.0f, 1.0f);
	for (size_t bone_idx = 0; bone_idx < bones.size(); ++bone_idx)
	{
		auto& result = bones[bone_idx].relative_transform;
		const auto& source = state.fade->source_pose[bone_idx];
		const auto& target = target_pose[bone_idx];
		result.set_pos(glm::mix(source.get_pos(), target.get_pos(), blend));
		result.set_scale(glm::mix(source.get_scale(), target.get_scale(), blend));
		auto target_orientation = target.get_orient();
		if (glm::dot(source.get_orient(), target_orientation) < 0.0f)
			target_orientation = -target_orientation;
		result.set_orient(glm::normalize(glm::slerp(
			source.get_orient(), target_orientation, blend)));
	}
}

AnimationID SkeletalAnimationSystem::add_skeletal_animation(
	const std::string& name,
	std::vector<BoneAnimation>&& bone_animations,
	SkeletalRigSignature rig_signature,
	std::string source)
{
	for (auto& bone_animation : bone_animations)
		align_rotation_track_hemisphere(bone_animation.rotation_track);

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

bool SkeletalAnimationSystem::remove_skeletal_animation(const AnimationID animation_id)
{
	if (!animations.contains(animation_id))
		return false;

	std::vector<SkeletonID> affected_skeletons;
	for (const auto& [skeleton_id, active_animation] : active_animations)
		if (active_animation == animation_id)
			affected_skeletons.push_back(skeleton_id);
	for (const auto skeleton_id : affected_skeletons)
		stop_animation(skeleton_id);

	animations.erase(animation_id);
	ResourceProvenance::erase_animation(animation_id);
	return true;
}

bool SkeletalAnimationSystem::play_animation(
	const SkeletonID skeleton_id,
	const AnimationID animation_id,
	const bool loop)
{
	if (!get_ecs().has_skeleton(skeleton_id) || !animations.contains(animation_id)
		|| !is_animation_compatible(skeleton_id, animation_id))
		return false;
	auto& component = get_ecs().get_skeletal_component(skeleton_id);
	for (auto& bone : component.get_bones())
		bone.relative_transform = bone.original_transform;
	active_animations.insert_or_assign(skeleton_id, animation_id);
	AnimationState state;
	state.should_loop = loop;
	state.current_animation_elapsed_secs = 0.0f;
	animation_states[skeleton_id] = state;
	return true;
}

bool SkeletalAnimationSystem::crossfade_animation(
	const SkeletonID skeleton_id,
	const AnimationID animation_id,
	const float transition_secs,
	const bool loop)
{
	if (transition_secs <= 0.0f)
		return play_animation(skeleton_id, animation_id, loop);
	if (!get_ecs().has_skeleton(skeleton_id) || !animations.contains(animation_id)
		|| !is_animation_compatible(skeleton_id, animation_id))
		return false;

	AnimationState::Fade fade;
	fade.duration_secs = transition_secs;
	auto& component = get_ecs().get_skeletal_component(skeleton_id);
	const auto& bones = component.get_bones();
	fade.source_pose.reserve(bones.size());
	for (const auto& bone : bones)
		fade.source_pose.push_back(bone.relative_transform);

	active_animations.insert_or_assign(skeleton_id, animation_id);
	AnimationState state;
	state.should_loop = loop;
	state.fade = std::move(fade);
	animation_states[skeleton_id] = std::move(state);
	return true;
}

void SkeletalAnimationSystem::stop_animation(const SkeletonID skeleton_id)
{
	auto& component = get_ecs().get_skeletal_component(skeleton_id);
	for (auto& bone : component.get_bones())
		bone.relative_transform = bone.original_transform;
	active_animations.erase(skeleton_id);
	animation_states.erase(skeleton_id);
}

void SkeletalAnimationSystem::set_animation_looping(const SkeletonID skeleton_id, const bool looping)
{
	if (const auto state = animation_states.find(skeleton_id); state != animation_states.end())
		state->second.should_loop = looping;
}

void SkeletalAnimationSystem::set_animation_paused(const SkeletonID skeleton_id, const bool paused)
{
	if (const auto state = animation_states.find(skeleton_id); state != animation_states.end())
		state->second.paused = paused;
}

void SkeletalAnimationSystem::set_animation_speed(const SkeletonID skeleton_id, const float speed)
{
	if (const auto state = animation_states.find(skeleton_id); state != animation_states.end())
		state->second.playback_speed = speed;
}

void SkeletalAnimationSystem::step_animation(const SkeletonID skeleton_id, const float delta_secs)
{
	const auto active = active_animations.find(skeleton_id);
	const auto state = animation_states.find(skeleton_id);
	if (active == active_animations.end() || state == animation_states.end())
		return;
	seek_animation(skeleton_id, state->second.current_animation_elapsed_secs + delta_secs);
}

void SkeletalAnimationSystem::seek_animation(const SkeletonID skeleton_id, const float elapsed_secs)
{
	const auto active = active_animations.find(skeleton_id);
	const auto state = animation_states.find(skeleton_id);
	if (active == active_animations.end() || state == animation_states.end())
		return;
	auto& animation = animations.at(active->second);
	state->second.current_animation_elapsed_secs = std::clamp(
		elapsed_secs, 0.0f, animation_duration(animation));

	apply_animation_pose(skeleton_id);
}

float SkeletalAnimationSystem::get_animation_duration(const AnimationID animation_id) const
{
	return animation_duration(animations.at(animation_id));
}

std::optional<SkeletalAnimationSystem::AnimationPlayback>
SkeletalAnimationSystem::get_animation_playback(const SkeletonID skeleton_id) const
{
	const auto active = active_animations.find(skeleton_id);
	const auto state = animation_states.find(skeleton_id);
	if (active == active_animations.end() || state == animation_states.end())
		return std::nullopt;
	return AnimationPlayback{
		.animation_id = active->second,
		.looping = state->second.should_loop,
		.paused = state->second.paused,
		.elapsed_secs = state->second.current_animation_elapsed_secs,
		.duration_secs = animation_duration(animations.at(active->second)),
		.speed = state->second.playback_speed,
	};
}

void SkeletalAnimationSystem::remove_entity(Entity id) 
{
	if (const auto skeleton_id = get_ecs().get_skeleton_id(id))
	{
		active_animations.erase(*skeleton_id);
		animation_states.erase(*skeleton_id);
	}
}
