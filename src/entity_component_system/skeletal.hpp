#pragma once

#include "objects/object_id.hpp"
#include "shared_data_structures.hpp"
#include "maths.hpp"


using Entity = ObjectID;

struct Bone
{
	Maths::Transform relative_transform;
	uint32_t parent_node;
};

struct SkeletalComponent
{
public:
	SkeletalComponent(const std::vector<Bone>& bones) : bones(bones) {}

	Bone& get_bone(const uint32_t node_id) { return bones[node_id]; }
	const Bone& get_bone(const uint32_t node_id) const { return bones[node_id]; }

	std::vector<Bone>& get_bones() { return bones; }
	const std::vector<Bone>& get_bones() const { return bones; }

	// This system assumes theres always exactly 1 root bone and it exists at index 0
	Bone& get_root_bone() { return bones[0]; }

	std::vector<SDS::Bone> get_bones_data() const;

private:
	std::vector<Bone> bones;
};

class SkeletalSystem
{
public:
	void add_bones(Entity id, const std::vector<Bone>& bones) { skeletons.emplace(id, bones); }
	void remove_bones(Entity id) { skeletons.erase(id); }
	std::vector<SDS::Bone> get_bones(Entity id) const { return skeletons.at(id).get_bones_data(); }

protected:
	void remove_entity(Entity id) { remove_bones(id); }

private:
	std::unordered_map<Entity, SkeletalComponent> skeletons;
};