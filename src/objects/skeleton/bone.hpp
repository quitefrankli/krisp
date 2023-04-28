/*
	It's not clear currently what the plan is when it comes to skeletons
	The intuition is that we need something to group a collection of related objects together
	This would also be useful for animations e.t.c.
*/

// #pragma once

// #include "objects/generic_objects.hpp"

// #include <vector>


// class SkeletonJoint;

// class SkeletonBone : public GenericClickableObject
// {
// public:
// 	SkeletonBone(Shape&& shape) : GenericClickableObject(std::move(shape)) {}
// 	SkeletonBone(std::vector<ShapePtr>&& shapes) : GenericClickableObject(std::move(shapes)) {}
// 	void add_child_joint(SkeletonJoint& joint, const glm::vec3& pos1, const glm::vec3& pos2);
// 	virtual void on_click(OnClickDispatchers::IBaseDispatcher& dispatcher, const Maths::Ray& ray, const glm::vec3& intersection) override;

// private:
// 	std::vector<Connection> child_joints;
// };