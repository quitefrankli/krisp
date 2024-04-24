#pragma once

#include "game_engine.hpp"

#include "camera.hpp"
#include "objects/objects.hpp"
#include "objects/cubemap.hpp"
#include "graphics_engine/graphics_engine.hpp"
#include "graphics_engine/graphics_engine_commands.hpp"
#include "utility.hpp"
#include "analytics.hpp"
#include "interface/gizmo.hpp"
#include "hot_reload.hpp"
#include "gui/gui_manager.hpp"
#include "experimental.hpp"
#include "iapplication.hpp"

#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <quill/Quill.h>
#include <fmt/core.h>
#include <fmt/color.h>

#include <iostream>
#include <vector>
#include <thread>
#include <chrono>


static DummyApplication dummy_app;

template<template<typename> typename GraphicsEngineTemplate>
GameEngine<GraphicsEngineTemplate>::GameEngine(std::function<void()>&& restart_signaller, App::Window& window) :
	restart_signaller(std::move(restart_signaller)),
	window(window),
	mouse(std::make_unique<Mouse<GameEngine>>(window)),
	gizmo(std::make_unique<Gizmo<GameEngine>>(*this)),
	graphics_engine(std::make_unique<GraphicsEngineT>(*this)),
	camera(std::make_unique<Camera>(Listener(), 
		   static_cast<float>(window.get_width())/static_cast<float>(window.get_height())))
{
	window.setup_callbacks(*this);
	camera->look_at(Maths::zero_vec, glm::vec3(0.0f, 3.0f, -3.0f));
	draw_object(camera->focus_obj);
	draw_object(camera->upvector_obj);
	experimental = std::make_unique<Experimental<GameEngine>>(*this);
	spawn_object<CubeMap>(); // background/horizon

	auto light_source = std::make_shared<Object>(ShapeFactory::sphere());
	light_source->set_name("light source");
	light_source->get_shapes()[0]->set_material(Material::create_material(EMaterialType::LIGHT_SOURCE));
	auto& light_source_obj = spawn_object(std::move(light_source));
	light_source_obj.set_position(glm::vec3(0.0f, 5.0f, 0.0f));
	ecs.add_light_source(light_source_obj.get_id(), LightComponent());
	ecs.add_collider(light_source_obj.get_id(), std::make_unique<SphereCollider>());
	ecs.add_clickable_entity(light_source_obj.get_id());

	get_gui_manager().template spawn_gui<GuiMusic<GameEngine>>(audio_engine.create_source());
	TPS_counter = std::make_unique<Analytics>([this](float tps) {
		set_tps(float(1e6) / tps);
	}, 1);
	TPS_counter->text = "TPS Counter";
}

template<template<typename> typename GraphicsEngineTemplate>
GameEngine<GraphicsEngineTemplate>::GameEngine(App::Window& window)  :
	window(window),
	mouse(std::make_unique<Mouse<GameEngine>>(window)),
	gizmo(std::make_unique<Gizmo<GameEngine>>(*this)),
	graphics_engine(std::make_unique<GraphicsEngineT>(*this)),
	camera(std::make_unique<Camera>(Listener(), 
		   static_cast<float>(window.get_width())/static_cast<float>(window.get_height())))
{
	window.setup_callbacks(*this);
	camera->look_at(Maths::zero_vec, glm::vec3(0.0f, 3.0f, -3.0f));
	draw_object(camera->focus_obj);
	draw_object(camera->upvector_obj);
	experimental = std::make_unique<Experimental<GameEngine>>(*this);
	spawn_object<CubeMap>(); // background/horizon

	auto light_source = std::make_shared<Object>(ShapeFactory::sphere());
	light_source->set_name("light source");
	light_source->get_shapes()[0]->set_material(Material::create_material(EMaterialType::LIGHT_SOURCE));
	auto& light_source_obj = spawn_object(std::move(light_source));
	light_source_obj.set_position(glm::vec3(0.0f, 5.0f, 0.0f));
	ecs.add_light_source(light_source_obj.get_id(), LightComponent());
	ecs.add_collider(light_source_obj.get_id(), std::make_unique<SphereCollider>());
	ecs.add_clickable_entity(light_source_obj.get_id());

	get_gui_manager().template spawn_gui<GuiMusic<GameEngine>>(audio_engine.create_source());
	TPS_counter = std::make_unique<Analytics>([this](float tps) {
		set_tps(1e6 / tps);
	}, 1);
	TPS_counter->text = "TPS Counter";
}

template<template<typename> typename GraphicsEngineTemplate>
void GameEngine<GraphicsEngineTemplate>::run()
{
	graphics_engine_thread = std::thread(&GraphicsEngineT::run, graphics_engine.get());
	Utility::sleep(std::chrono::milliseconds(100));

	if (!application)
	{
		application = &dummy_app;
	}

	try 
	{
		application->on_begin();
		gizmo->init();

		Analytics analytics(60);
		analytics.text = "GameEngine: avg loop processing period (excluding sleep)";

		std::chrono::time_point<std::chrono::system_clock> time = std::chrono::system_clock::now();

		TPS_counter->start();
		Utility::LoopSleeper loop_sleeper(std::chrono::milliseconds(17));
		while (!should_shutdown && !window.should_close())
		{
	#ifndef DISABLE_SLEEP
			loop_sleeper();
	#endif
			const std::chrono::time_point<std::chrono::system_clock> new_time = std::chrono::system_clock::now();
			std::chrono::duration<float, std::milli> chrono_time_delta = new_time - time;
			const float time_delta = chrono_time_delta.count() * 0.001; // in seconds
			time = new_time;
			analytics.start();

			// for ticks per second
			TPS_counter->stop();
			TPS_counter->start();
			main_loop(time_delta);

			analytics.stop();
		}
    } catch (const std::exception& e) { // if an exception occurs in the game engine we need to cleanly shutdown graphics_engine first
		fmt::print(fg(fmt::color::red), "Exception Thrown!: {}\n", e.what());
	} catch (...) {
		fmt::print(fg(fmt::color::red), "Exception Thrown!: UNKNOWN\n");
	}

	shutdown();
	graphics_engine_thread.join();
}

template<template<typename> typename GraphicsEngineTemplate>
void GameEngine<GraphicsEngineTemplate>::main_loop(const float time_delta)
{
	window.poll_events();

	process_objs_to_delete();

	// poll gui stuff, we should take advantage of polymorphism later on, but for now this is relatively simple
	get_gui_manager().process(*this);

	if (mouse->mmb_down)
	{
		if (window.is_shift_down())
		{
			// panning
			const float min_threshold = 0.01f;
			mouse->update_pos();
			const glm::vec2 offset_vec = mouse->get_orig_offset();
			const float magnitude = glm::length(offset_vec);
			if (magnitude > min_threshold)
			{
				camera->pan(offset_vec, magnitude);
			}
		} else
		{
			// orbiting
			const float min_threshold = 0.001f;
			mouse->update_pos();
			glm::vec2 offset = mouse->get_prev_offset();
			float magnitude = glm::length(offset);
			if (magnitude > min_threshold)
			{
				camera->rotate_camera(offset, time_delta);
			}
		}
	} else if (mouse->lmb_down)
	{
		const float sensitivity = 2.0f;
		const float min_threshold = 0.01f;

		// check w/ IMMEDIATE prev pos if offset is big enough
		if (mouse->update_pos_on_significant_offset(min_threshold))
		{
			const auto offset = mouse->get_orig_offset();
			glm::vec2 screen_axis(offset.x, offset.y);
			float magnitude = glm::length(screen_axis);

			const Maths::Ray r1 = camera->get_ray(mouse->orig_pos);
			const Maths::Ray r2 = camera->get_ray(mouse->curr_pos);
			gizmo->process(r1, r2);
		}
	}

	ecs.process(time_delta);
	experimental->process(time_delta);
	application->on_tick(time_delta);
}

template<template<typename> typename GraphicsEngineTemplate>
void GameEngine<GraphicsEngineTemplate>::shutdown_impl()
{
	if (should_shutdown)
	{
		return;
	}

	should_shutdown = true;
	graphics_engine->enqueue_cmd(std::make_unique<ShutdownCmd>());
}

template<template<typename> typename GraphicsEngineTemplate>
void GameEngine<GraphicsEngineTemplate>::process_objs_to_delete()
{
	const auto curr_deleted_objs_count_in_graphics_engine = graphics_engine->get_num_objs_deleted();
	while (!entity_deletion_queue.empty())
	{
		const auto& obj = entity_deletion_queue.front();

		if (obj.second >= curr_deleted_objs_count_in_graphics_engine)
		{
			break;
		}
		
		if (ecs.has_skeletal_component(obj.first))
		{
			const auto& bone_visualisers = ecs.get_skeletal_component(obj.first).get_visualisers();
			for (const auto bone_visualiser : bone_visualisers)
			{
				delete_object(bone_visualiser);
			}
		}

		objects.erase(obj.first);
		ecs.remove_object(obj.first);
		entity_deletion_queue.pop();
	}
}

template<template<typename> typename GraphicsEngineTemplate>
GameEngine<GraphicsEngineTemplate>::~GameEngine()
{
}

template<template<typename> typename GraphicsEngineTemplate>
inline Object& GameEngine<GraphicsEngineTemplate>::spawn_object(std::shared_ptr<Object>&& object)
{
	auto it = objects.emplace(object->get_id(), std::move(object));
	Object& new_obj = *(it.first->second);
	ecs.add_object(new_obj);
	send_graphics_cmd(std::make_unique<SpawnObjectCmd>(it.first->second));

	return new_obj;
}

template<template<typename> typename GraphicsEngineTemplate>
Object& GameEngine<GraphicsEngineTemplate>::spawn_skinned_object(std::shared_ptr<Object>&& object,
                                                                 std::vector<Bone>&& bones)
{
	object->set_render_type(EPipelineType::SKINNED);
	ecs.add_bones(object->get_id(), bones);
	std::vector<ObjectID> visualisers;
	const glm::quat bone_rotator = glm::angleAxis(-Maths::PI/2.0f, Maths::right_vec);
	for (const auto& bone : bones)
	{
		Object& obj = spawn_object<Object>(ShapeFactory::arrow());
		for (auto& shape : obj.get_shapes())
		{
			// gltf models have bones pointing upwards by default
			shape->transform_vertices(bone_rotator);
		}
		obj.set_visibility(false);
		visualisers.push_back(obj.get_id());
	}
	ecs.add_bone_visualisers(object->get_id(), visualisers);
	
	return spawn_object(std::move(object));
}

template<template<typename> typename GraphicsEngineTemplate>
void GameEngine<GraphicsEngineTemplate>::delete_object(ObjectID id)
{
	if (!entity_deletion_queue.push(id))
	{
		return;
	}

	ecs.remove_clickable_entity(id);
	graphics_engine->enqueue_cmd(std::make_unique<DeleteObjectCmd>(id));
}

template<template<typename> typename GraphicsEngineTemplate>
void GameEngine<GraphicsEngineTemplate>::highlight_object(const Object& object)
{
	graphics_engine->enqueue_cmd(std::make_unique<StencilObjectCmd>(object));
}

template<template<typename> typename GraphicsEngineTemplate>
void GameEngine<GraphicsEngineTemplate>::unhighlight_object(const Object& object)
{
	graphics_engine->enqueue_cmd(std::make_unique<UnStencilObjectCmd>(object));
}

template<template<typename> typename GraphicsEngineTemplate>
GuiManager<GameEngine<GraphicsEngineTemplate>>& GameEngine<GraphicsEngineTemplate>::get_gui_manager()
{
	return graphics_engine->get_gui_manager();
}

template<template<typename> typename GraphicsEngineTemplate>
void GameEngine<GraphicsEngineTemplate>::restart()
{
	restart_signaller();
	shutdown();
}

template<template<typename> typename GraphicsEngineTemplate>
void GameEngine<GraphicsEngineTemplate>::send_graphics_cmd(std::unique_ptr<GraphicsEngineCommand>&& cmd)
{
	graphics_engine->enqueue_cmd(std::move(cmd));
}

template<template<typename> typename GraphicsEngineTemplate>
uint32_t GameEngine<GraphicsEngineTemplate>::get_window_width()
{
	return window.get_width();
}

template<template<typename> typename GraphicsEngineTemplate>
uint32_t GameEngine<GraphicsEngineTemplate>::get_window_height()
{
	return window.get_height();
}

template<template<typename> typename GraphicsEngineTemplate>
Maths::Ray GameEngine<GraphicsEngineTemplate>::get_mouse_ray() const
{
	return camera->get_ray(mouse->get_curr_pos());
}

template<template<typename> typename GraphicsEngineTemplate>
void GameEngine<GraphicsEngineTemplate>::preview_objs_in_gui(
	const std::vector<Object*>& objs, 
	GuiPhotoBase& gui_window) 
{
	send_graphics_cmd(std::make_unique<PreviewObjectsCmd>(objs, gui_window));
}
