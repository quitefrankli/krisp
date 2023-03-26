#pragma once

#include "game_engine.hpp"
#include "hot_reload.hpp"
#include "objects/cubemap.hpp"
#include "objects/light_source.hpp"
#include "objects/objects.hpp"
#include "resource_loader.hpp"
#include "utility.hpp"
#include "objects/skeleton/skeleton.hpp"
#include "shapes/shape_factory.hpp"

#include <iostream>


template<typename GameEngineT>
class Experimental
{
public:
	Experimental(GameEngineT& engine) : 
		engine(engine)
		// skeleton(
		// 	[&engine](std::shared_ptr<Object>&& object)
		// 	{
		// 		IClickable* clickable = dynamic_cast<IClickable*>(object.get());
		// 		if (clickable)
		// 		{
		// 			engine.add_clickable(object->get_id(), clickable);
		// 		}
		// 		engine.spawn_object(std::move(object));
		// 	},
		// 	[&engine](uint64_t id)
		// 	{
		// 		engine.delete_object(id);
		// 	}
		// )
	{
		// SkeletonBone& root = skeleton.new_bone();
		// engine.add_clickable(root.get_id(), &root);
		// SkeletonBone& arm = skeleton.new_bone();
		// arm.set_scale({1.0f, 5.0f, 1.0f});
		// arm.set_position({0.0f, 2.5f, 0.0f});
		// SkeletonBone& hand = skeleton.new_bone();
		// hand.set_scale({5.0f, 1.0f, 1.0f});
		// hand.set_position({2.5f, 5.0f, 0.0f});
		// SkeletonBone& finger = skeleton.new_bone();
		// finger.set_scale({1.0f, 5.0f, 1.0f});
		// finger.set_position({5.0f, 7.5f, 0.0f});

		// // connect them all together
		// SkeletonJoint* joint = nullptr;
		// skeleton.set_root(&root);

		// joint = &skeleton.new_joint();
		// root.add_child_joint(*joint, {}, {});
		// joint->add_child_bone(arm, {}, {});

		// joint = &skeleton.new_joint();
		// joint->set_position({0.0f, 5.0f, 0.0f});
		// arm.add_child_joint(*joint, {}, {});
		// joint->add_child_bone(hand, {}, {});
		
		// joint = &skeleton.new_joint();
		// joint->set_position({5.0f, 5.0f, 0.0f});
		// hand.add_child_joint(*joint, {}, {});
		// joint->add_child_bone(finger, {}, {});
	}

	// manually triggered
	void process()
	{
		engine.get_graphics_engine().enqueue_cmd(
			std::make_unique<UpdateRayTracingCmd>()
		);
	}

	// game engine triggers this periodically
	// time_delta is in seconds
	void process(float time_delta)
	{
		time_elapsed += time_delta;
		if (time_elapsed > 1.0f)
		{
			time_elapsed = 0;
		}
		
		// skeleton.process(time_delta);
	}

private:
	GameEngineT& engine;
	// Skeleton skeleton;
	float time_elapsed = 0;
};