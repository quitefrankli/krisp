#include "game_engine.hpp"

#include "camera.hpp"
#include "objects.hpp"
#include "shapes.hpp"
#include "graphics_engine/graphics_engine.hpp"
#include "graphics_engine/graphics_engine_commands.hpp"
#include "utility_functions.hpp"
#include "analytics.hpp"

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
	std::thread graphics_engine_thread(&GraphicsEngine::run, graphics_engine.get());
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	Analytics analytics;
	analytics.text = "GameEngine: average cycle ms";

	const std::string mesh = "../resources/models/viking_room.obj";
	const std::string texture = "../resources/textures/viking_room.png";
	// spawn_object<Object>(resource_loader, mesh, texture);

	// spawn_object<Cube>("../resources/textures/texture2.jpg");
	spawn_object<Cube>("../resources/textures/texture.jpg");
	// spawn_object<Object>(resource_loader, "../resources/models/object.obj", "../resources/textures/object.png");

	while (!should_shutdown && !glfwWindowShouldClose(get_window()))
	{
		std::chrono::time_point<std::chrono::system_clock> new_time = std::chrono::system_clock::now();
		std::chrono::duration<float, std::milli> chrono_delta_time = new_time - time;
		const float delta_time = chrono_delta_time.count();
		time = new_time;

		analytics.start();

		glfwPollEvents();

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
				glm::mat4 final_transform = glm::mat4_cast(quaternion) * orig; // order matters! 
				objects[0]->set_transform(final_transform);
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

		std::this_thread::sleep_for(std::chrono::milliseconds(10));

		analytics.stop();
	}

	shutdown();
	graphics_engine_thread.join();
}

void GameEngine::handle_window_callback(GLFWwindow* glfw_window, int key, int scan_code, int action, int mode)
{
	GameEngine* engine = static_cast<GameEngine*>(glfwGetWindowUserPointer(glfw_window));
	engine->handle_window_callback_impl(glfw_window, key, scan_code, action, mode);
}

static int inc = 0;

void GameEngine::handle_window_callback_impl(GLFWwindow*, int key, int scan_code, int action, int mode)
{
	auto pressed_key = glfwGetKeyName(key, scan_code);

	printf("input detected [%d], key:=%d, scan_code:=%d, action:=%d, mode:=%d, translated_key:=%s\n", 
		   inc++, key, scan_code, action, mode, pressed_key ? pressed_key : "N/A");

	// if (glfwGetKey(window.get_window(), key) == GLFW_RELEASE)
	// {
	// 	return; // ignore key releases
	// }

	// if (action == GLFW_REPEAT)
	// {
	// 	return; // ignore held keys
	// }

	if (action == GLFW_RELEASE)
	{
		return; // ignore held keys
	}

	glm::vec3 pos;
	switch (key)
	{
		case GLFW_KEY_ESCAPE:
			shutdown();
			break;

		case GLFW_KEY_P:
			pos = get_camera().get_position();
			pos.z += 0.5f;
			get_camera().set_position(pos);
			break;

		case GLFW_KEY_L:
			pos = get_camera().get_position();
			pos.z -= 0.5f;
			get_camera().set_position(pos);
			break;		

		case GLFW_KEY_LEFT:
			break;

		case GLFW_KEY_RIGHT:
			break;

		case GLFW_KEY_S:
		{
			auto& obj = spawn_object<Cube>("../resources/textures/texture.jpg");
			obj.set_position(glm::vec3(1.0f));
			break;
		}
		case GLFW_KEY_X: // experimental
		{
			// auto& obj = spawn_object<Object>(resource_loader, "../resources/models/viking_room.obj", "../resources/textures/viking_room.png");
			// obj.set_position(glm::vec3(-1.0f));
			auto& obj = spawn_object<Cube>();
			obj.set_position(glm::vec3(-1.0f));
			break;
		}
		case GLFW_KEY_F: // wireframe mode
		{
			graphics_engine->enqueue_cmd(std::make_unique<ToggleWireFrameModeCmd>());
			break;
		}
		default:
			break;
	}
}

void GameEngine::handle_window_resize_callback(GLFWwindow* glfw_window, int width, int height) {
	reinterpret_cast<GameEngine*>(glfwGetWindowUserPointer(glfw_window))->handle_window_resize_callback_impl(glfw_window, width, height);
}

void GameEngine::handle_window_resize_callback_impl(GLFWwindow* glfw_window, int width, int height)
{
	// graphics_engine->set_frame_buffer_resized();
	// TODO handle resizing of window
}

void GameEngine::handle_mouse_button_callback(GLFWwindow* glfw_window, int button, int action, int mode)
{
	reinterpret_cast<GameEngine*>(glfwGetWindowUserPointer(glfw_window))->handle_mouse_button_callback_impl(glfw_window, button, action, mode);
}

void GameEngine::handle_mouse_button_callback_impl(GLFWwindow* glfw_window, int button, int action, int mode)
{
	if (button == GLFW_MOUSE_BUTTON_RIGHT)
	{
		if (action == GLFW_PRESS)
		{
			mouse.rmb_down = true;
			mouse.update_pos();
			tracker.update(*camera);
		} else if (action == GLFW_RELEASE)
		{
			mouse.rmb_down = false;
		}
	} else if (button == GLFW_MOUSE_BUTTON_LEFT)
	{
		if (action == GLFW_PRESS)
		{
			mouse.lmb_down = true;
			mouse.update_pos();
			mouse.orig_pos = mouse.curr_pos;
			tracker.update(*objects[0]);
		} else if (action == GLFW_RELEASE)
		{
			mouse.lmb_down = false;
		}
	} else if (button == GLFW_MOUSE_BUTTON_MIDDLE)
	{
		if (action == GLFW_PRESS)
		{
			mouse.mmb_down = true;
			mouse.update_pos();
			mouse.orig_pos = mouse.curr_pos;
			tracker.update(*camera);
			camera->prev_focus = camera->focus;
		} else if (action == GLFW_RELEASE)
		{
			mouse.mmb_down = false;
		}
	}
}

void GameEngine::handle_scroll_callback(GLFWwindow* glfw_window, double xoffset, double yoffset)
{
	reinterpret_cast<GameEngine*>(glfwGetWindowUserPointer(glfw_window))->handle_scroll_callback_impl(glfw_window, xoffset, yoffset);
}

void GameEngine::handle_scroll_callback_impl(GLFWwindow* glfw_window, double xoffset, double yoffset)
{
	const float sensitivity = -0.2f;
	glm::vec3 axis(0.0f, 0.0f, yoffset * sensitivity);
	glm::mat4 transform = glm::translate(camera->get_transform(), axis);
	camera->set_transform(transform);
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

template<typename Object_T, typename... Args>
Object_T& GameEngine::spawn_object(Args&&... args)
{
	objects.push_back(std::make_shared<Object_T>(std::forward<Args>(args)...));
	auto& object = objects.back();
	SpawnObjectCmd cmd;
	cmd.object = object;
	cmd.object_id = object->get_id();
	graphics_engine->enqueue_cmd(std::make_unique<SpawnObjectCmd>(std::move(cmd)));
	return *static_cast<Object_T*>(object.get());
}

GameEngine::~GameEngine() = default;

void ObjectPositionTracker::update(Object& object)
{
	position = object.get_position();
	scale = object.get_scale();
	rotation = object.get_rotation();
	transform = object.get_transform();
}