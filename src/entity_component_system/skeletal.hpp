#pragma once

#include "identifications.hpp"
#include "shared_data_structures.hpp"
#include "maths.hpp"

#include <string>
#include <vector>


using Entity = ObjectID;

class ECS;

struct Bone
{
	Maths::Transform original_transform;
	Maths::Transform relative_transform;
	Maths::Transform inverse_bind_pose;
	std::string name;
	uint32_t parent_node;
};

struct BoneAnimation
{
	struct KeyFrame 
	{
		Maths::Transform transform;
		float animation_stage_secs;
	};

	float animation_start_secs = std::numeric_limits<float>::max();;
	float animation_end_secs = std::numeric_limits<float>::min();;
	std::vector<KeyFrame> key_frames;

	// returns false if animation_stage_secs is out of range
	bool get_transform(const float animation_stage_secs, Maths::Transform& out_transform) const;
};

struct SkeletalAnimation
{
	std::vector<BoneAnimation> bone_animations;
	std::string name;
};

struct SkeletalComponent
{
public:
	SkeletalComponent() = default;
	SkeletalComponent(const std::vector<Bone>& bones) : bones(bones) {}

	std::vector<Bone>& get_bones() { return bones; }
	const std::vector<Bone>& get_bones() const { return bones; }
	std::vector<SDS::Bone> get_bones_data() const;

private:
	std::vector<Bone> bones;
};

class SkeletalSystem
{
public:
	virtual ECS& get_ecs() = 0;

	SkeletonID add_skeleton(const std::vector<Bone>& bones);
	std::vector<SDS::Bone> get_bones(SkeletonID id) const { return skeletons.at(id).get_bones_data(); }
	SkeletalComponent& get_skeletal_component(SkeletonID id) { return skeletons.at(id); }

protected:
	void remove_entity(Entity id);

private:
	std::unordered_map<MeshID, SkeletonID> mesh_to_skeleton;
	std::unordered_map<SkeletonID, SkeletalComponent> skeletons;
};

class SkeletalAnimationSystem
{
public:
	virtual ECS& get_ecs() = 0;

	void process(const float delta_secs);

	AnimationID add_skeletal_animation(const std::string& name, std::vector<BoneAnimation>&& bone_animations);
	void play_animation(SkeletonID skeleton_id, AnimationID animation_id, bool loop = false);
	const std::unordered_map<AnimationID, SkeletalAnimation>& get_skeletal_animations() const { return animations; }

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