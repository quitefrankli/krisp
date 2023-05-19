#pragma once

#include "game_engine.hpp"
#include "hot_reload.hpp"
#include "objects/cubemap.hpp"
#include "objects/objects.hpp"
#include "resource_loader.hpp"
#include "utility.hpp"
#include "shapes/shape_factory.hpp"

#include <iostream>


template<typename GameEngineT>
class Experimental
{
public:
	Experimental(GameEngineT& engine) : 
		engine(engine)
	{
	}

	// manually triggered
	void process()
	{
		// const std::string model_name = "simple_test_model.gltf";
		const std::string model_name = "skellyjack.gltf";
		// const std::string model_name = "donut.gltf";
		auto res = ResourceLoader::get().load_model((Utility::get().get_model_path()/model_name).string());
		auto obj = std::make_shared<Object>(std::move(res.shapes));
		auto entity_id = obj->get_id();
		underlying_entity_id = obj->get_id().get_underlying();
		engine.spawn_skinned_object(std::move(obj), std::move(res.bones));
		ECS& ecs = engine.get_ecs();
		ecs.add_collider(entity_id, std::make_unique<SphereCollider>());
		ecs.add_clickable_entity(entity_id);
		for (auto& [name, animation] : res.animations)
		{
			ecs.add_skeletal_animation(entity_id, name, std::move(animation));
		}
		std::string anim = ecs.get_skeletal_animations(entity_id)[0];
		ecs.animate_skeleton(entity_id, anim);
	}

	// game engine triggers this periodically
	// time_delta is in seconds
	void process(float time_delta)
	{
		time_elapsed += time_delta;
		// if (time_elapsed > 1.0f)
		// {
		// 	time_elapsed = 0;
		// }
		
		if (underlying_entity_id == -1)
		{
			return;
		}

		// SkeletalComponent& comp = engine.get_ecs().get_skeletal_component(ObjectID(underlying_entity_id));
		// static auto orig_transform = comp.get_bone(1).relative_transform;
		// float new_angle = std::sinf(time_elapsed) * Maths::PI/4.0f;
		// glm::quat new_orient = glm::angleAxis(new_angle, Maths::forward_vec) * orig_transform.get_orient();
		// comp.get_bone(1).relative_transform.set_orient(new_orient);
	}

private:
	GameEngineT& engine;
	float time_elapsed = 0;
	int underlying_entity_id = -1;
};