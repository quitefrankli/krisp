#include "game_engine.hpp"

#include "camera.hpp"
#include "objects/objects.hpp"
#include "graphics_engine/graphics_engine.hpp"
#include "graphics_engine/graphics_engine_commands.hpp"
#include "utility_functions.hpp"
#include "analytics.hpp"
#include "simulations/tower_of_hanoi.hpp"
#include "hot_reload.hpp"
#include "gui/gui_manager.hpp"
#include "experimental.hpp"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <quill/Quill.h>

#include <iostream>
#include <vector>
#include <thread>
#include <chrono>


extern quill::Logger* logger;

GameEngine::GameEngine(std::function<void()>&& restart_signaller) :
	restart_signaller(std::move(restart_signaller)),
	window(this),
	mouse(window),
	gizmo(*this)
{
	graphics_engine = std::make_unique<GraphicsEngine>(*this);

	camera = std::make_unique<Camera>(*this, graphics_engine->get_window_width<float>() / graphics_engine->get_window_height<float>());
	experimental = std::make_unique<Experimental>(*this);

	graphics_engine->setup();
}

void GameEngine::run()
{
	graphics_engine_thread = std::thread(&GraphicsEngine::run, graphics_engine.get());
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	gizmo.init();

	Analytics analytics;
	analytics.text = "GameEngine: average cycle ms";

	auto& light = spawn_object<LightSource>(glm::vec3(1.0f));
	light.set_position(glm::vec3(0.0f, 2.0f, 2.0f));

	// spawn_object<Object>(resource_loader, mesh, texture);

	// spawn_object<Cube>("../resources/textures/texture2.jpg");
	// spawn_object<Cube>("../resources/textures/texture.jpg");

	// glm::quat model_rotation = glm::angleAxis(
	// 	Maths::PI, 
	// 	glm::vec3(0.0f, 1.0f, 0.0f)) * glm::angleAxis(-Maths::PI/2.0f, glm::vec3(1.0f, 0.0f, 0.0f));
	// spawn_object<Object>(
	// 	resource_loader, 
	// 	"../resources/models/object1.obj", 
	// 	"../resources/textures/object1.png",
	// 	glm::mat4_cast(model_rotation)).set_position(glm::vec3(0.0f, -8.0f, -2.0f));

	// spawn_object<>(
	// 	resource_loader,
	// 	"../resources/models/viking_room.obj",
	// 	"../resources/textures/viking_room.png"
	// );

	// analytics.quick_timer_start();
	// spawn_object<Sphere>();
	// spawn_object<Cube>();
	// spawn_object<HollowCylinder>();
	// analytics.quick_timer_stop();

	while (!should_shutdown && !glfwWindowShouldClose(get_window()))
	{
		std::chrono::time_point<std::chrono::system_clock> new_time = std::chrono::system_clock::now();
		std::chrono::duration<float, std::milli> chrono_delta_time = new_time - time;
		const float delta_time = chrono_delta_time.count();
		time = new_time;
		analytics.start();

		glfwPollEvents();

		// poll gui stuff, we should take advantage of polymorphism later on, but for now this is relatively simple
		get_gui_manager().process(*this);

		if (mouse.rmb_down)
		{
			const float min_threshold = 0.001f;
			mouse.update_pos();
			glm::vec2 offset = mouse.get_prev_offset();
			float magnitude = glm::length(offset);
			if (magnitude > min_threshold)
			{
				camera->rotate_camera(offset, delta_time);
			}
		} else if (mouse.lmb_down)
		{
			const float sensitivity = 2.0f;
			const float min_threshold = 0.01f;

			// check w/ IMMEDIATE prev pos if offset is big enough
			if (mouse.update_pos_on_significant_offset(min_threshold))
			{
				const auto offset = mouse.get_orig_offset();
				glm::vec2 screen_axis(offset.x, offset.y);
				float magnitude = glm::length(screen_axis);

				const Maths::Ray r1 = camera->get_ray(mouse.orig_pos);
				const Maths::Ray r2 = camera->get_ray(mouse.curr_pos);
				gizmo.process(r1, r2);
				
				// glm::vec3 axis = camera->sync_to_camera(screen_axis);
				// gizmo.process(axis, magnitude);


				// if (tracker.object)
				// {
				// 	// flip the two axes for rotation
				// 	 // the fact that this is -ve is very strange might have to do with our coordinate system
				// 	axis = camera->sync_to_camera(glm::vec2{-screen_axis.y,screen_axis.x});
				// 	glm::quat quaternion = glm::angleAxis(magnitude, axis);
				// 	glm::mat4 orig = tracker.transform;

				// 	glm::quat new_rot = quaternion * tracker.rotation; // order matters! 
				// 	tracker.object->set_rotation(new_rot);
				// }
			}
		} else if (mouse.mmb_down)
		{
			const float sensitivity = 1.0f;
			const float min_threshold = 0.01f;
			mouse.update_pos();
			auto offset_vec = mouse.get_orig_offset();
			float magnitude = glm::length(offset_vec);
			if (magnitude > min_threshold)
			{
				magnitude *= sensitivity;
				glm::vec3 axis = camera->sync_to_camera(offset_vec) * magnitude; // might not need magnitude here
				camera->look_at(camera->get_old_focus() + axis);
			}
		}

		animator.process(delta_time / 1e3);
		experimental->process(delta_time / 1e3);
		std::this_thread::sleep_for(std::chrono::milliseconds(10));

		analytics.stop();
	}

	shutdown();
	graphics_engine_thread.join();
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

void GameEngine::delete_object(obj_id_t id)
{
	auto it_obj = objects.find(id);
	if (it_obj == objects.end())
	{
		LOG_ERROR(logger, "GameEngine::delete_object: attempted to delete non-existant object");
		return;
	}
	objects.erase(it_obj);
	graphics_engine->enqueue_cmd(std::make_unique<DeleteObjectCmd>(id));
}

GuiManager& GameEngine::get_gui_manager()
{
	return static_cast<GuiManager&>(graphics_engine->get_gui_manager());
}

void GameEngine::restart()
{
	restart_signaller();
	shutdown();
}

void GameEngine::send_graphics_cmd(std::unique_ptr<GraphicsEngineCommand>&& cmd)
{
	graphics_engine->enqueue_cmd(std::move(cmd));
}