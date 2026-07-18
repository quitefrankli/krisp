#include "skeletal.hpp"
#include "ecs.hpp"
#include "serialization/serialization_helpers.hpp"

#include <algorithm>
#include <functional>

namespace
{
std::string interpolation_name(const BoneAnimation::Interpolation interpolation)
{
	switch (interpolation) {
	case BoneAnimation::Interpolation::LINEAR: return "linear";
	case BoneAnimation::Interpolation::STEP: return "step";
	case BoneAnimation::Interpolation::CUBIC_SPLINE: return "cubic_spline";
	}
	throw SerializationError("Unsupported skeletal interpolation");
}

BoneAnimation::Interpolation read_interpolation(const Deserializer& in)
{
	const auto interpolation = in.child("interpolation");
	const auto value = interpolation.as<std::string>();
	if (value == "linear") return BoneAnimation::Interpolation::LINEAR;
	if (value == "step") return BoneAnimation::Interpolation::STEP;
	if (value == "cubic_spline") return BoneAnimation::Interpolation::CUBIC_SPLINE;
	throw SerializationError("Unsupported interpolation value '" + value + "' at " + interpolation.path());
}

template<typename T, typename Writer>
void write_track(Serializer& out, const std::string_view key,
	const BoneAnimation::Track<T>& track, Writer writer)
{
	auto map = out.map(key);
	map.write("interpolation", interpolation_name(track.interpolation));
	auto keys = map.sequence("keys");
	for (const auto& track_key : track.keys) {
		auto entry = keys.append_map();
		entry.write("time", track_key.animation_stage_secs);
		writer(entry, "value", track_key.value);
		writer(entry, "in_tangent", track_key.in_tangent);
		writer(entry, "out_tangent", track_key.out_tangent);
	}
}

template<typename T, typename Reader>
BoneAnimation::Track<T> read_track(const Deserializer& in, const std::string_view key, Reader reader)
{
	const auto map = in.child(key);
	BoneAnimation::Track<T> track;
	track.interpolation = read_interpolation(map);
	const auto key_entries = map.child("keys").elements();
	for (std::size_t index = 0; index < key_entries.size(); ++index) {
		const auto& entry = key_entries[index];
		BoneAnimation::TrackKey<T> track_key;
		track_key.animation_stage_secs = entry.read<float>("time");
		track_key.value = reader(entry, "value");
		track_key.in_tangent = reader(entry, "in_tangent");
		track_key.out_tangent = reader(entry, "out_tangent");
		if (!track.keys.empty() && track_key.animation_stage_secs <= track.keys.back().animation_stage_secs)
			throw SerializationError("Skeletal track times must be strictly increasing at "
				+ map.child("keys").path() + "[" + std::to_string(index) + "].time");
		track.keys.push_back(track_key);
	}
	return track;
}

void validate_bones(const std::vector<Bone>& bones, const std::string& path)
{
	std::vector<uint8_t> state(bones.size());
	std::function<void(std::size_t)> visit = [&](const std::size_t index) {
		if (state[index] == 1)
			throw SerializationError("Cyclic bone parent at " + path + "[" + std::to_string(index) + "].parent_index");
		if (state[index] == 2)
			return;
		state[index] = 1;
		const auto parent = bones[index].parent_node;
		if (parent != Bone::NO_PARENT) {
			if (parent >= bones.size())
				throw SerializationError("Invalid bone parent at " + path + "[" + std::to_string(index) + "].parent_index");
			visit(parent);
		}
		state[index] = 2;
	};
	for (std::size_t index = 0; index < bones.size(); ++index)
		visit(index);
}
}

void SkeletalSystem::serialize(Serializer& out) const
{
	std::vector<SkeletonID> ids;
	for (const auto& [id, _] : skeletons) ids.push_back(id);
	std::ranges::sort(ids);
	auto entries = out.sequence("skeletal_system");
	for (const auto id : ids) {
		auto entry = entries.append_map();
		entry.write("skeleton_id", id.get_underlying());
		auto bones_out = entry.sequence("bones");
		for (const auto& bone : skeletons.at(id).get_bones()) {
			auto bone_out = bones_out.append_map();
			bone_out.write("name", bone.name);
			if (bone.parent_node == Bone::NO_PARENT) bone_out.write_null("parent_index");
			else bone_out.write("parent_index", bone.parent_node);
			EcsSerialization::write_transform(bone_out, "original_transform", bone.original_transform);
			EcsSerialization::write_transform(bone_out, "relative_transform", bone.relative_transform);
			EcsSerialization::write_transform(bone_out, "inverse_bind_pose", bone.inverse_bind_pose);
		}
	}
}

void SkeletalSystem::deserialize(const Deserializer& in)
{
	std::unordered_map<SkeletonID, SkeletalComponent> restored;
	const auto entries = in.child("skeletal_system").elements();
	for (std::size_t index = 0; index < entries.size(); ++index) {
		const auto& entry = entries[index];
		const SkeletonID id(entry.read<std::uint64_t>("skeleton_id"));
		std::vector<Bone> bones;
		const auto bone_entries = entry.child("bones").elements();
		for (const auto& bone_in : bone_entries) {
			Bone bone;
			bone.name = bone_in.read<std::string>("name");
			const auto parent = bone_in.child("parent_index");
			bone.parent_node = parent.kind() == SerializationKind::Null ? Bone::NO_PARENT : parent.as<std::uint32_t>();
			bone.original_transform = EcsSerialization::read_transform(bone_in, "original_transform");
			bone.relative_transform = EcsSerialization::read_transform(bone_in, "relative_transform");
			bone.inverse_bind_pose = EcsSerialization::read_transform(bone_in, "inverse_bind_pose");
			bones.push_back(std::move(bone));
		}
		validate_bones(bones, "$.skeletal_system[" + std::to_string(index) + "].bones");
		if (!restored.emplace(id, SkeletalComponent(bones)).second)
			throw SerializationError("Duplicate skeleton ID at $.skeletal_system[" + std::to_string(index) + "].skeleton_id");
	}
	if (!restored.empty()) {
		const auto maximum = std::ranges::max_element(restored, {}, [](const auto& entry) {
			return entry.first.get_underlying();
		})->first.get_underlying();
		if (maximum == std::numeric_limits<std::uint64_t>::max())
			throw SerializationError("Cannot advance SkeletonID counter beyond uint64 maximum");
		SkeletonID::set_next_id(std::max(SkeletonID::get_next_id(), maximum + 1));
	}
	skeletons = std::move(restored);
}

void SkeletalAnimationSystem::serialize(Serializer& out) const
{
	auto system = out.map("skeletal_animation_system");
	std::vector<AnimationID> ids;
	for (const auto& [id, _] : animations) ids.push_back(id);
	std::ranges::sort(ids);
	auto animations_out = system.sequence("animations");
	for (const auto id : ids) {
		const auto& animation = animations.at(id);
		auto entry = animations_out.append_map();
		entry.write("animation_id", id.get_underlying());
		entry.write("name", animation.name);
		entry.write("source", animation.source);
		auto signature = entry.sequence("rig_signature");
		for (const auto& bone : animation.rig_signature) {
			auto bone_out = signature.append_map();
			bone_out.write("name", bone.name);
			bone_out.write("parent_name", bone.parent_name);
		}
		auto bone_animations = entry.sequence("bone_animations");
		for (const auto& bone : animation.bone_animations) {
			auto bone_out = bone_animations.append_map();
			bone_out.write("animation_start_secs", bone.animation_start_secs);
			bone_out.write("animation_end_secs", bone.animation_end_secs);
			EcsSerialization::write_transform(bone_out, "base_transform", bone.base_transform);
			auto frames = bone_out.sequence("key_frames");
			for (const auto& frame : bone.key_frames) {
				auto frame_out = frames.append_map();
				frame_out.write("time", frame.animation_stage_secs);
				EcsSerialization::write_transform(frame_out, "transform", frame.transform);
			}
			write_track(bone_out, "translation_track", bone.translation_track, EcsSerialization::write_vec3);
			write_track(bone_out, "rotation_track", bone.rotation_track, EcsSerialization::write_vec4);
			write_track(bone_out, "scale_track", bone.scale_track, EcsSerialization::write_vec3);
		}
	}

	std::vector<SkeletonID> active_ids;
	for (const auto& [id, _] : active_animations) active_ids.push_back(id);
	std::ranges::sort(active_ids);
	auto active_out = system.sequence("active_animations");
	for (const auto skeleton_id : active_ids) {
		const auto state = animation_states.find(skeleton_id);
		if (state == animation_states.end())
			throw SerializationError("Missing active animation state");
		auto entry = active_out.append_map();
		entry.write("skeleton_id", skeleton_id.get_underlying());
		entry.write("animation_id", active_animations.at(skeleton_id).get_underlying());
		entry.write("should_loop", state->second.should_loop);
		entry.write("elapsed_secs", state->second.current_animation_elapsed_secs);
	}
	if (animation_states.size() != active_animations.size())
		throw SerializationError("Dangling skeletal animation state");
}

void SkeletalAnimationSystem::deserialize(const Deserializer& in)
{
	const auto system = in.child("skeletal_animation_system");
	std::unordered_map<AnimationID, SkeletalAnimation> restored_animations;
	const auto animation_entries = system.child("animations").elements();
	for (std::size_t index = 0; index < animation_entries.size(); ++index) {
		const auto& entry = animation_entries[index];
		const AnimationID id(entry.read<std::uint64_t>("animation_id"));
		SkeletalAnimation animation;
		animation.name = entry.read<std::string>("name");
		animation.source = entry.read<std::string>("source");
		for (const auto& bone : entry.child("rig_signature").elements())
			animation.rig_signature.push_back({ bone.read<std::string>("name"), bone.read<std::string>("parent_name") });
		for (const auto& bone_in : entry.child("bone_animations").elements()) {
			BoneAnimation bone;
			bone.animation_start_secs = bone_in.read<float>("animation_start_secs");
			bone.animation_end_secs = bone_in.read<float>("animation_end_secs");
			bone.base_transform = EcsSerialization::read_transform(bone_in, "base_transform");
			for (const auto& frame_in : bone_in.child("key_frames").elements())
				bone.key_frames.push_back({ EcsSerialization::read_transform(frame_in, "transform"), frame_in.read<float>("time") });
			bone.translation_track = read_track<glm::vec3>(bone_in, "translation_track", EcsSerialization::read_vec3);
			bone.rotation_track = read_track<glm::vec4>(bone_in, "rotation_track", EcsSerialization::read_vec4);
			bone.scale_track = read_track<glm::vec3>(bone_in, "scale_track", EcsSerialization::read_vec3);
			animation.bone_animations.push_back(std::move(bone));
		}
		if (!restored_animations.emplace(id, std::move(animation)).second)
			throw SerializationError("Duplicate animation ID at $.skeletal_animation_system.animations["
				+ std::to_string(index) + "].animation_id");
	}

	std::unordered_map<SkeletonID, AnimationID> restored_active;
	std::unordered_map<SkeletonID, AnimationState> restored_states;
	const auto active_entries = system.child("active_animations").elements();
	for (std::size_t index = 0; index < active_entries.size(); ++index) {
		const auto& entry = active_entries[index];
		const SkeletonID skeleton_id(entry.read<std::uint64_t>("skeleton_id"));
		const AnimationID animation_id(entry.read<std::uint64_t>("animation_id"));
		const auto animation = restored_animations.find(animation_id);
		if (animation == restored_animations.end())
			throw SerializationError("Unknown animation ID at $.skeletal_animation_system.active_animations["
				+ std::to_string(index) + "].animation_id");
		try {
			const auto& bones = get_ecs().get_skeletal_component(skeleton_id).get_bones();
			if (animation->second.bone_animations.size() != bones.size()
				|| animation->second.rig_signature != make_skeletal_rig_signature(bones))
				throw SerializationError("Incompatible active animation at $.skeletal_animation_system.active_animations["
					+ std::to_string(index) + "].animation_id");
		} catch (const std::out_of_range&) {
			throw SerializationError("Unknown skeleton ID at $.skeletal_animation_system.active_animations["
				+ std::to_string(index) + "].skeleton_id");
		}
		AnimationState state;
		state.should_loop = entry.read<bool>("should_loop");
		state.current_animation_elapsed_secs = entry.read<float>("elapsed_secs");
		if (!restored_active.emplace(skeleton_id, animation_id).second)
			throw SerializationError("Duplicate active skeleton at $.skeletal_animation_system.active_animations["
				+ std::to_string(index) + "].skeleton_id");
		restored_states.emplace(skeleton_id, state);
	}
	if (!restored_animations.empty()) {
		const auto maximum = std::ranges::max_element(restored_animations, {}, [](const auto& entry) {
			return entry.first.get_underlying();
		})->first.get_underlying();
		if (maximum == std::numeric_limits<std::uint64_t>::max())
			throw SerializationError("Cannot advance AnimationID counter beyond uint64 maximum");
		AnimationID::set_next_id(std::max(AnimationID::get_next_id(), maximum + 1));
	}
	animations = std::move(restored_animations);
	active_animations = std::move(restored_active);
	animation_states = std::move(restored_states);
}
