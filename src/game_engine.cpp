#include "game_engine.hpp"

#include "camera.hpp"
#include "objects.hpp"
#include "shapes.hpp"
#include "graphics_engine/graphics_engine.hpp"
#include "graphics_engine/graphics_engine_commands.hpp"
#include "utility_functions.hpp"
#include "analytics.hpp"
#include "simulations/tower_of_hanoi.hpp"
#include "hot_reload.hpp"
#include "gui/gui_manager.hpp"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <quill/Quill.h>

#include <iostream>
#include <vector>
#include <thread>
#include <chrono>


extern quill::Logger* logger;

GameEngine::GameEngine() :
	window(this),
	mouse(window)
{
	graphics_engine = std::make_unique<GraphicsEngine>(*this);

	camera = std::make_unique<Camera>(*this, graphics_engine->get_window_width<float>() / graphics_engine->get_window_height<float>());

	graphics_engine->setup();
}

void GameEngine::run()
{
	graphics_engine_thread = std::thread(&GraphicsEngine::run, graphics_engine.get());
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	Analytics analytics;
	analytics.text = "GameEngine: average cycle ms";

	// spawn_object<Object>(resource_loader, mesh, texture);

	// spawn_object<Cube>("../resources/textures/texture2.jpg");
	// spawn_object<Cube>("../resources/textures/texture.jpg");

	// glm::quat model_rotation = glm::angleAxis(
	// 	Maths::PI, 
	// 	glm::vec3(0.0f, 1.0f, 0.0f)) * glm::angleAxis(-Maths::PI/2.0f, glm::vec3(1.0f, 0.0f, 0.0f));
	// spawn_object<Object>(
	// 	resource_loader, 
	// 	"../resources/models/object.obj", 
	// 	"../resources/textures/object.png",
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

	// spawn_object<Cube>();

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
			const float sensitivity = 0.2f;
			const float min_threshold = 0.001f;
			mouse.update_pos();
			auto offset = mouse.get_prev_offset();
			glm::vec2 screen_axis(-offset.y, offset.x); // the fact that this is -ve is very strange might have to do with our coordinate system
			float magnitude = glm::length(screen_axis);
			if (magnitude > min_threshold) {
				magnitude *= delta_time * sensitivity;
				
				glm::vec3 axis = camera->sync_to_camera(screen_axis);

				glm::quat quaternion = glm::angleAxis(magnitude, axis);
				glm::mat4 curr = camera->get_transform();
				glm::mat4 transform = glm::mat4_cast(quaternion);

				// for rotating about arbitrary axis
				auto cur_focus = camera->focus;
				curr[3] -= glm::vec4(camera->focus, 0.0f);
				glm::mat4 final_transform = transform * curr;
				final_transform[3] += glm::vec4(camera->focus, 0.0f);

				camera->set_transform(final_transform);
			}
		} else if (mouse.lmb_down)
		{
			const float sensitivity = 2.0f;
			const float min_threshold = 0.01f;
			mouse.update_pos();
			auto offset = mouse.get_orig_offset();
			glm::vec2 screen_axis(-offset.y, offset.x); // the fact that this is -ve is very strange might have to do with our coordinate system
			float magnitude = glm::length(screen_axis);
			if (std::fabsf(magnitude) > min_threshold && !objects.empty()) {
				glm::vec3 axis = camera->sync_to_camera(screen_axis);
				glm::quat quaternion = glm::angleAxis(magnitude, axis);
				glm::mat4 orig = tracker.transform;

				glm::quat new_rot = quaternion * tracker.rotation; // order matters! 
				objects[0]->set_rotation(new_rot);
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
				camera->focus = camera->prev_focus + axis;

				camera->set_position(tracker.position + axis);
				camera->focus = camera->prev_focus + axis;

				// this entire function should be pan and moved into camera
				// for camera focus object
				camera->focus_obj->set_position(camera->focus);
			}
		}

		animator.process(delta_time / 1e3);
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

void ObjectPositionTracker::update(Object& object)
{
	position = object.get_position();
	scale = object.get_scale();
	rotation = object.get_rotation();
	transform = object.get_transform();
}

Maths::Ray GameEngine::screen_to_world(glm::vec2 screen)
{
	auto view_mat = camera->get_view();
	auto proj_mat = camera->get_perspective();
	proj_mat[1][1] *= -1.0f; // our world is upside down
	auto proj_view_mat = glm::inverse(proj_mat * view_mat);

	auto unproj = [&](const float depth)
	{
		auto point = proj_view_mat * glm::vec4(mouse.curr_pos, depth, 1.0f);
		point /= point.w;
		return glm::vec3(point);
	};

	auto p1 = unproj(0.7f);
	auto p2 = unproj(0.95f);

	return Maths::Ray(camera->get_position(), glm::normalize(p2-p1));
}

GuiManager& GameEngine::get_gui_manager()
{
	return static_cast<GuiManager&>(graphics_engine->get_gui_manager());
}