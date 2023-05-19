#pragma once

#include "objects/object_id.hpp"
#include "shared_data_structures.hpp"
#include "maths.hpp"


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

struct SkeletalComponent
{
public:
	SkeletalComponent() = default;
	SkeletalComponent(const std::vector<Bone>& bones) : bones(bones) {}

	Bone& get_bone(const uint32_t node_id) { return bones[node_id]; }
	const Bone& get_bone(const uint32_t node_id) const { return bones[node_id]; }

	std::vector<Bone>& get_bones() { return bones; }
	const std::vector<Bone>& get_bones() const { return bones; }

	// This system assumes theres always exactly 1 root bone and it exists at index 0
	Bone& get_root_bone() { return bones[0]; }

	std::vector<SDS::Bone> get_bones_data() const;

	const std::vector<Entity>& get_visualisers() const { return visualisers; }
	void set_visualisers(const std::vector<Entity>& visualisers);

	void add_animation(const std::string& name, std::vector<BoneAnimation>&& animations) 
	{ 
		this->animations.emplace(name, std::move(animations)); 
	}
	void set_animation(const std::string& animation_name);
	void animate(const float delta_secs);

	std::vector<std::string> get_animations() const;

private:
	std::vector<Bone> bones;
	std::vector<Entity> visualisers;
	std::unordered_map<std::string, std::vector<BoneAnimation>> animations;
	std::string current_animation;
	bool should_loop_animation = true;
	float current_animation_elapsed_secs = 0.0f;
};

// TODO: split this into a skeletal system and a skeletal animation system
class SkeletalSystem
{
public:
	virtual ECS& get_ecs() = 0;

	bool has_skeletal_component(Entity id) const { return skeletons.find(id) != skeletons.end(); }
	void add_bones(Entity id, const std::vector<Bone>& bones) { skeletons.emplace(id, bones); }
	void remove_bones(Entity id);
	std::vector<SDS::Bone> get_bones(Entity id) const { return skeletons.at(id).get_bones_data(); }
	std::vector<Entity> get_all_skinned_entities() const;
	SkeletalComponent& get_skeletal_component(Entity id) { return skeletons.at(id); }
	const SkeletalComponent& get_skeletal_component(Entity id) const { return skeletons.at(id); }
	void add_bone_visualisers(Entity id, const std::vector<Entity>& bones);
	void animate_skeleton(Entity id, const std::string& animation_name);
	void add_skeletal_animation(Entity id, const std::string& name, std::vector<BoneAnimation>&& animations)
	{
		skeletons[id].add_animation(name, std::move(animations));
	}
	std::vector<std::string> get_skeletal_animations(Entity id) const;
	void process(const float delta_secs);

protected:
	void remove_entity(Entity id) { remove_bones(id); }

private:
	std::unordered_map<Entity, SkeletalComponent> skeletons;
};