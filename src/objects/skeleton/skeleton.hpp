/*
	It's not clear currently what the plan is when it comes to skeletons
	The intuition is that we need something to group a collection of related objects together
	This would also be useful for animations e.t.c.
*/

// #pragma once

// #include "joint.hpp"
// #include "bone.hpp"

// #include <vector>
// #include <functional>


// class Skeleton
// {
// public:
// 	// these functors aim to reduce coupling with the engine
// 	// if the spawner is passed a nullptr clickable, then it should still spawn the object
// 	using obj_spawner_t = std::function<void(std::shared_ptr<Object>&&)>;
// 	using obj_remover_t = std::function<void(ObjectID)>;

// 	// the ctor accepts an obj_spawner that tells game_engine to spawn an object
// 	Skeleton(obj_spawner_t spawner, obj_remover_t remover);
// 	virtual ~Skeleton();

// 	SkeletonBone* get_root() { return root; }
// 	void set_root(SkeletonBone* root) { this->root = root; }

// private:
// 	const obj_spawner_t obj_spawner;
// 	const obj_remover_t obj_remover;
// 	// std::vector<std::unique_ptr<SkeletonJoint>> joints;
// 	SkeletonBone* root; // every skeleton must have a root bone where all other bones/joints are attached to either directly or indirectly
// };