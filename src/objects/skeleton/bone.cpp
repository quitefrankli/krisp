// #include "bone.hpp"
// #include "joint.hpp"

// #include <iostream>


// void SkeletonBone::on_click(OnClickDispatchers::IBaseDispatcher& dispatcher, const Maths::Ray& ray, const glm::vec3& intersection)
// {
// 	IClickable::on_click(dispatcher, ray, intersection);
// }

// void SkeletonBone::add_child_joint(SkeletonJoint& joint, const glm::vec3& pos1, const glm::vec3& pos2)
// {
// 	child_joints.push_back({joint, pos1, pos2});
// 	joint.attach_to(this);
// }