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
#include "entity_component_system/material_system.hpp"
#include "renderable/material_factory.hpp"
#include "entity_component_system/mesh_system.hpp"
#include "renderable/mesh_factory.hpp"

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

GameEngine::GameEngine(App::Window& window) :
	GameEngine(window, [](GameEngine& engine) { return std::make_unique<GraphicsEngine>(engine); })
{
}

GameEngine::GameEngine(App::Window& window, GraphicsEngineFactory graphics_engine_factory) :
	window(window),
	mouse(std::make_unique<Mouse>(window)),
	gizmo(std::make_unique<Gizmo>(*this)),
	graphics_engine(graphics_engine_factory(*this)),
	camera(std::make_unique<Camera>(Listener(), 
		   static_cast<float>(window.get_width())/static_cast<float>(window.get_height()))),
	ecs(ECS::get())
{
	window.setup_callbacks(*this);
	camera->look_at(Maths::zero_vec, glm::vec3(0.0f, 3.0f, -3.0f));
	draw_object(camera->focus_obj);
	draw_object(camera->upvector_obj);
	experimental = std::make_unique<Experimental>(*this);
	spawn_object<CubeMap>(); // background/horizon

	Renderable light_renderable;
	const auto mat_id = MaterialFactory::fetch_preset(EMaterialPreset::LIGHT_SOURCE);
	light_renderable.material_ids.push_back(mat_id);
	light_renderable.mesh_id = MeshFactory::sphere_id();
	auto light_source = std::make_shared<Object>(light_renderable);
	light_source->set_name("light source");
	auto& light_source_obj = spawn_object(std::move(light_source));
	light_source_obj.set_position(glm::vec3(0.0f, 5.0f, 0.0f));
	ecs.add_light_source(light_source_obj.get_id(), LightComponent());
	ecs.add_collider(light_source_obj.get_id(), std::make_unique<SphereCollider>());
	ecs.add_clickable_entity(light_source_obj.get_id());

	get_gui_manager().template spawn_gui<GuiMusic>(audio_engine.create_source());
	TPS_counter = std::make_unique<Analytics>([this](float tps) {
		set_tps(float(1e6) / tps);
	}, 1);
	TPS_counter->text = "TPS Counter";
}

void GameEngine::run()
{
	graphics_engine_thread = std::thread(&GraphicsEngineBase::run, graphics_engine.get());
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

void GameEngine::main_loop(const float time_delta)
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

void GameEngine::shutdown_impl()
{
	if (should_shutdown)
	{
		return;
	}

	should_shutdown = true;
	graphics_engine->enqueue_cmd(std::make_unique<ShutdownCmd>());
}

GameEngine::~GameEngine() = default;

inline Object& GameEngine::spawn_object(std::shared_ptr<Object>&& object)
{
	auto it = objects.emplace(object->get_id(), std::move(object));
	Object& new_obj = *(it.first->second);
	ecs.add_object(new_obj);

	// TODO: fix visualisers if skinned object
	// std::vector<ObjectID> visualisers;
	// const glm::quat bone_rotator = glm::angleAxis(-Maths::PI/2.0f, Maths::right_vec);
	// for (const auto& bone : bones)
	// {
	// 	Object& obj = spawn_object<Object>(ShapeFactory::arrow());
	// 	for (auto& shape : obj.get_shapes())
	// 	{
	// 		// gltf models have bones pointing upwards by default
	// 		shape->transform_vertices(bone_rotator);
	// 	}
	// 	obj.set_visibility(false);
	// 	visualisers.push_back(obj.get_id());
	// }
	// ecs.add_bone_visualisers(object->get_id(), visualisers);

	send_graphics_cmd(std::make_unique<SpawnObjectCmd>(it.first->second));

	return new_obj;
}

void GameEngine::delete_object(ObjectID id)
{
	if (!entity_deletion_queue.push(id))
	{
		return;
	}

	graphics_engine->enqueue_cmd(std::make_unique<DeleteObjectCmd>(id));
}

void GameEngine::process_objs_to_delete()
{
	const auto curr_deleted_objs_count_in_graphics_engine = graphics_engine->get_num_objs_deleted();
	DestroyResourcesCmd destroy_resources_cmd;
	while (!entity_deletion_queue.empty())
	{
		const auto& obj = entity_deletion_queue.front();
		const ObjectID id = obj.first;

		if (obj.second >= curr_deleted_objs_count_in_graphics_engine)
		{
			break;
		}

		// TODO: fix visualisers		
		// if (ecs.has_skeletal_component(id))
		// {
		// 	const auto& bone_visualisers = ecs.get_skeletal_component(id).get_visualisers();
		// 	for (const auto bone_visualiser : bone_visualisers)
		// 	{
		// 		delete_object(bone_visualiser);
		// 	}
		// }
		
		ecs.remove_clickable_entity(id);
		const Object& object = *get_object(id);
		for (const auto& renderable : object.renderables)
		{
			for (const auto& mat_id : renderable.material_ids)
			{
				if (MaterialSystem::unregister_owner(mat_id) == 0)
				{
					destroy_resources_cmd.material_ids.push_back(mat_id);
				}
			}
			
			if (MeshSystem::unregister_owner(renderable.mesh_id) == 0)
			{
				destroy_resources_cmd.mesh_ids.push_back(renderable.mesh_id);
			}
		}

		send_graphics_cmd(std::make_unique<DestroyResourcesCmd>(destroy_resources_cmd));

		objects.erase(id);
		ecs.remove_object(id);
		entity_deletion_queue.pop();
	}
}

void GameEngine::highlight_object(const Object& object)
{
	graphics_engine->enqueue_cmd(std::make_unique<StencilObjectCmd>(object));
}

void GameEngine::unhighlight_object(const Object& object)
{
	graphics_engine->enqueue_cmd(std::make_unique<UnStencilObjectCmd>(object));
}

GuiManager& GameEngine::get_gui_manager()
{
	return graphics_engine->get_gui_manager();
}

void GameEngine::send_graphics_cmd(std::unique_ptr<GraphicsEngineCommand>&& cmd)
{
	graphics_engine->enqueue_cmd(std::move(cmd));
}

uint32_t GameEngine::get_window_width()
{
	return window.get_width();
}

uint32_t GameEngine::get_window_height()
{
	return window.get_height();
}

Maths::Ray GameEngine::get_mouse_ray() const
{
	return camera->get_ray(mouse->get_curr_pos());
}

void GameEngine::preview_objs_in_gui(
	const std::vector<Object*>& objs, 
	GuiPhotoBase& gui_window) 
{
	send_graphics_cmd(std::make_unique<PreviewObjectsCmd>(objs, gui_window));
}

Gizmo& GameEngine::get_gizmo() { return *gizmo; }
