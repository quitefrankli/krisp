#pragma once

#include "game_engine.hpp"
#include "hot_reload.hpp"
#include "objects/cubemap.hpp"
#include "objects/objects.hpp"
#include "resource_loader.hpp"
#include "utility.hpp"
#include "shapes/shape_factory.hpp"
#include "objects/generic_objects.hpp"

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
		// const std::string model_name = "skeletal_ecs_tests.gltf";
		const std::string model_name = "skellyjack_without_materials.gltf";
		// const std::string model_name = "donut.gltf";
		auto res = ResourceLoader::get().load_model((Utility::get().get_model_path()/model_name).string());
		auto obj = std::make_shared<GenericClickableObject>(std::move(res.shapes));
		engine.add_clickable(obj->get_id(), obj.get());
		engine.spawn_skinned_object(std::move(obj), std::move(res.bones));
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
		
	}

private:
	GameEngineT& engine;
	float time_elapsed = 0;
};