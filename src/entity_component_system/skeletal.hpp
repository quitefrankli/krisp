#pragma once

#include "identifications.hpp"
#include "shared_data_structures.hpp"
#include "maths.hpp"

#include <string>
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

	struct KeyFrame 
	{
		Maths::Transform transform;
		float animation_stage_secs;
	};

	float animation_start_secs = std::numeric_limits<float>::max();
	float animation_end_secs = std::numeric_limits<float>::lowest();
	std::vector<KeyFrame> key_frames;
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

struct SkeletalComponent
{
public:
	SkeletalComponent() = default;
	SkeletalComponent(const std::vector<Bone>& bones) : bones(bones) {}

	std::vector<Bone>& get_bones() { return bones; }
	const std::vector<Bone>& get_bones() const { return bones; }
	// Bone transforms after hierarchy composition, before inverse bind-pose
	// multiplication. These are suitable for gameplay pose adjustments such as IK.
	std::vector<glm::mat4> get_model_space_bone_transforms() const;
	std::vector<SDS::Bone> get_bones_data() const;

private:
	std::vector<Bone> bones;
};

class SkeletalSystem
{
public:
	virtual ECS& get_ecs() = 0;

	SkeletonID add_skeleton(const std::vector<Bone>& bones);
	void attach_skeleton(Entity id, SkeletonID skeleton_id);
	std::optional<SkeletonID> get_skeleton_id(Entity id) const;
	std::vector<SkeletonID> get_skeleton_ids() const;
	std::vector<SDS::Bone> get_bones(SkeletonID id) const { return skeletons.at(id).get_bones_data(); }
	SkeletalComponent& get_skeletal_component(SkeletonID id) { return skeletons.at(id); }
	const SkeletalComponent& get_skeletal_component(SkeletonID id) const { return skeletons.at(id); }
	void serialize(Serializer& out) const;
	void deserialize(const Deserializer& in);

protected:
	void remove_entity(Entity id);

private:
	std::unordered_map<SkeletonID, SkeletalComponent> skeletons;
	std::unordered_map<Entity, SkeletonID> entity_skeletons;
};

class SkeletalAnimationSystem
{
public:
	virtual ECS& get_ecs() = 0;
	virtual const ECS& get_ecs() const = 0;

	void process(const float delta_secs);

	AnimationID add_skeletal_animation(
		const std::string& name,
		std::vector<BoneAnimation>&& bone_animations,
		SkeletalRigSignature rig_signature,
		std::string source = {});
	void play_animation(SkeletonID skeleton_id, AnimationID animation_id, bool loop = false);
	bool is_animation_compatible(SkeletonID skeleton_id, AnimationID animation_id) const;
	const std::unordered_map<AnimationID, SkeletalAnimation>& get_skeletal_animations() const { return animations; }
	void serialize(Serializer& out) const;
	void deserialize(const Deserializer& in);

protected:
	void remove_entity(Entity id);

private:
	struct AnimationState
	{
		bool should_loop = false;
		float current_animation_elapsed_secs = 0.0f;
	};

	std::unordered_map<AnimationID, SkeletalAnimation> animations;
	std::unordered_map<SkeletonID, AnimationID> active_animations;
	std::unordered_map<SkeletonID, AnimationState> animation_states;
};
