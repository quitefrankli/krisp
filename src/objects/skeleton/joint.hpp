/*
	It's not clear currently what the plan is when it comes to skeletons
	The intuition is that we need something to group a collection of related objects together
	This would also be useful for animations e.t.c.
*/

// #pragma once

// #include "objects/object.hpp"
// #include "objects/generic_objects.hpp"
// #include "shapes/shape.hpp"

// #include <glm/vec3.hpp>

// #include <vector>


// class SkeletonBone;

// // The SkeletonJoint for a simple 360 degree rotation only joint
// class SkeletonJoint : public GenericClickableObject
// {
// public:
// 	SkeletonJoint(Shape&& shape) : GenericClickableObject(std::move(shape)) {}
// 	SkeletonJoint(std::vector<Shape>&& shapes) : GenericClickableObject(std::move(shapes)) {}
// 	void add_child_bone(SkeletonBone& bone, const glm::vec3& pos1, const glm::vec3& pos2);
// 	void update();
// 	virtual void set_rotation(const glm::quat& rotation) override;
// 	virtual void on_click(OnClickDispatchers::IBaseDispatcher& dispatcher, const Maths::Ray& ray, const glm::vec3& intersection) override {};


// private:
// 	struct Connection
// 	{
// 		SkeletonBone& bone;
// 		const glm::vec3 pos1; // the unscaled local pos of the joint
// 		const glm::vec3 pos2; // the unscaled local pos of the bone
// 	};
// 	std::vector<Connection> child_bones;
// };