#pragma once

#include "objects/object_id.hpp"
#include "shared_data_structures.hpp"
#include "maths.hpp"


using Entity = ObjectID;
class ECS;

struct Bone
{
	Maths::Transform relative_transform;
	Maths::Transform inverse_bind_pose;
	std::string name;
	uint32_t parent_node;
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

private:
	std::vector<Bone> bones;
	std::vector<Entity> visualisers;
};

class SkeletalSystem
{
public:
	virtual ECS& get_ecs() = 0;

	void add_bones(Entity id, const std::vector<Bone>& bones) { skeletons.emplace(id, bones); }
	void remove_bones(Entity id) { skeletons.erase(id); }
	std::vector<SDS::Bone> get_bones(Entity id) const { return skeletons.at(id).get_bones_data(); }
	std::vector<Entity> get_all_skinned_entities() const;
	const SkeletalComponent& get_skeletal_component(Entity id) const { return skeletons.at(id); }
	void add_bone_visualisers(Entity id, const std::vector<Entity>& bones);

	void process(const float delta_secs);

protected:
	void remove_entity(Entity id) { remove_bones(id); }

private:
	std::unordered_map<Entity, SkeletalComponent> skeletons;
};