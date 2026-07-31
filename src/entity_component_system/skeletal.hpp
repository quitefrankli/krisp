#pragma once

#include "identifications.hpp"
#include "shared_data_structures.hpp"
#include "maths.hpp"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>


using Entity = ObjectID;

class ECS;
class Serializer;
class Deserializer;

struct Bone
{
	static constexpr uint32_t NO_PARENT = std::numeric_limits<uint32_t>::max();
	Maths::Transform original_transform;
	Maths::Transform relative_transform;
	Maths::Transform inverse_bind_pose;
	std::string name;
	uint32_t parent_node = NO_PARENT;
};

struct BoneAnimation
{
	enum class Interpolation
	{
		LINEAR,
		STEP,
		CUBIC_SPLINE,
	};

	template<typename T>
	struct TrackKey
	{
		float animation_stage_secs = 0.0f;
		T value{};
		T in_tangent{};
		T out_tangent{};
	};

	template<typename T>
	struct Track
	{
		Interpolation interpolation = Interpolation::LINEAR;
		std::vector<TrackKey<T>> keys;
	};

	float animation_start_secs = std::numeric_limits<float>::max();
	float animation_end_secs = std::numeric_limits<float>::lowest();
	Maths::Transform base_transform;
	Track<glm::vec3> translation_track;
	Track<glm::vec4> rotation_track;
	Track<glm::vec3> scale_track;

	// returns false if animation_stage_secs is out of range
	bool get_transform(const float animation_stage_secs, Maths::Transform& out_transform) const;
};

struct SkeletalRigBone
{
	std::string name;
	std::string parent_name;
	auto operator<=>(const SkeletalRigBone&) const = default;
};

using SkeletalRigSignature = std::vector<SkeletalRigBone>;

SkeletalRigSignature make_skeletal_rig_signature(const std::vector<Bone>& bones);

struct SkeletalAnimation
{
	std::vector<BoneAnimation> bone_animations;
	std::string name;
	std::string source;
	SkeletalRigSignature rig_signature;
};

// A coherent render-facing copy captured while holding the pose mutex.
struct SkeletalRenderStateSnapshot
{
	std::vector<uint32_t> parent_indices;
	std::vector<glm::mat4> inverse_bind_poses;
	std::vector<glm::mat4> local_transforms;
};

struct SkeletalComponent
{
public:
	SkeletalComponent() = default;
	SkeletalComponent(const std::vector<Bone>& bones) : bones(bones) {}
	SkeletalComponent(const SkeletalComponent& other) : bones(other.bones) {}
	SkeletalComponent(SkeletalComponent&& other) noexcept : bones(std::move(other.bones)) {}
	SkeletalComponent& operator=(const SkeletalComponent&) = delete;
	SkeletalComponent& operator=(SkeletalComponent&&) = delete;

	std::vector<Bone>& get_bones() { return bones; }
	const std::vector<Bone>& get_bones() const { return bones; }
	// Bone transforms after hierarchy composition, before inverse bind-pose
	// multiplication. These are suitable for gameplay pose adjustments such as IK.
	std::vector<glm::mat4> get_model_space_bone_transforms() const;
	std::vector<SDS::Bone> get_bones_data() const;
	SkeletalRenderStateSnapshot snapshot_render_state() const;

private:
	std::vector<Bone> bones;
};

class SkeletalSystem
{
public:
	virtual ECS& get_ecs() = 0;
	virtual const ECS& get_ecs() const = 0;

	SkeletonID add_skeleton(const std::vector<Bone>& bones);
	bool remove_skeleton(SkeletonID id);
	std::vector<SkeletonID> get_skeleton_ids() const;
	bool has_skeleton(SkeletonID id) const { return skeletons.contains(id); }
	std::vector<SDS::Bone> get_bones(SkeletonID id) const { return skeletons.at(id).get_bones_data(); }
	SkeletalComponent& get_skeletal_component(SkeletonID id) { return skeletons.at(id); }
	const SkeletalComponent& get_skeletal_component(SkeletonID id) const { return skeletons.at(id); }
	void process(float delta_secs);
	// Keeps an entity aligned to a named bone. The exact source renderable
	// supplies both the skeleton binding and its composed visual transform.
	bool attach_entity_to_bone(
		Entity attached, RenderableID source_renderable, std::string_view bone_name,
		Maths::Transform local_transform = {});
	bool detach_entity_from_bone(Entity attached);
	void on_renderable_removed(RenderableID id);
	void serialize(Serializer& out) const;
	void deserialize(const Deserializer& in);
	void deserialize_bone_attachments(const Deserializer& in);

protected:
	void remove_entity(Entity id);

private:
	struct BoneAttachment
	{
		RenderableID source_renderable;
		uint32_t bone_index;
		Maths::Transform local_transform;
	};

	std::unordered_map<SkeletonID, SkeletalComponent> skeletons;
	std::unordered_map<Entity, BoneAttachment> bone_attachments;
};

class SkeletalAnimationSystem
{
public:
	static constexpr float DEFAULT_PLAYBACK_SPEED = 1.0f;

	struct AnimationPlayback
	{
		AnimationID animation_id;
		bool looping = false;
		bool paused = false;
		float elapsed_secs = 0.0f;
		float duration_secs = 0.0f;
		float speed = DEFAULT_PLAYBACK_SPEED;
	};

	virtual ECS& get_ecs() = 0;
	virtual const ECS& get_ecs() const = 0;

	void process(const float delta_secs);

	AnimationID add_skeletal_animation(
		const std::string& name,
		std::vector<BoneAnimation>&& bone_animations,
		SkeletalRigSignature rig_signature,
		std::string source = {});
	bool remove_skeletal_animation(AnimationID animation_id);
	bool play_animation(SkeletonID skeleton_id, AnimationID animation_id, bool loop = false);
	bool crossfade_animation(SkeletonID skeleton_id, AnimationID animation_id,
		float transition_secs, bool loop = false);
	void stop_animation(SkeletonID skeleton_id);
	void set_animation_looping(SkeletonID skeleton_id, bool looping);
	void set_animation_paused(SkeletonID skeleton_id, bool paused);
	void set_animation_speed(SkeletonID skeleton_id, float speed);
	void step_animation(SkeletonID skeleton_id, float delta_secs);
	void seek_animation(SkeletonID skeleton_id, float elapsed_secs);
	float get_animation_duration(AnimationID animation_id) const;
	std::optional<AnimationPlayback> get_animation_playback(SkeletonID skeleton_id) const;
	bool is_animation_compatible(SkeletonID skeleton_id, AnimationID animation_id) const;
	const std::unordered_map<AnimationID, SkeletalAnimation>& get_skeletal_animations() const { return animations; }
	void serialize(Serializer& out) const;
	void deserialize(const Deserializer& in);

private:
	struct AnimationState
	{
		struct Fade
		{
			std::vector<Maths::Transform> source_pose;
			float elapsed_secs = 0.0f;
			float duration_secs = 0.0f;
		};

		bool should_loop = false;
		bool paused = false;
		float playback_speed = DEFAULT_PLAYBACK_SPEED;
		float current_animation_elapsed_secs = 0.0f;
		std::optional<Fade> fade;
	};

	void apply_animation_pose(SkeletonID skeleton_id);

	std::unordered_map<AnimationID, SkeletalAnimation> animations;
	std::unordered_map<SkeletonID, AnimationID> active_animations;
	std::unordered_map<SkeletonID, AnimationState> animation_states;
};
