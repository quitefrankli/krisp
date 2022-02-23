#include "game_engine.hpp"

#include "camera.hpp"
#include "objects/objects.hpp"
#include "graphics_engine/graphics_engine.hpp"
#include "graphics_engine/graphics_engine_commands.hpp"
#include "utility_functions.hpp"
#include "analytics.hpp"
#include "simulations/tower_of_hanoi.hpp"
#include "hot_reload.hpp"
#include "experimental/experimental.hpp"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <quill/Quill.h>
#include <imgui.h>

#include <iostream>
#include <vector>
#include <thread>
#include <chrono>


void GameEngine::handle_window_callback(GLFWwindow* glfw_window, int key, int scan_code, int action, int mode)
{
	if (ImGui::GetIO().WantCaptureKeyboard)
	{
		return;
	}
	GameEngine* engine = static_cast<GameEngine*>(glfwGetWindowUserPointer(glfw_window));
	engine->handle_window_callback_impl(glfw_window, key, scan_code, action, mode);
}

static int inc = 0;

void GameEngine::handle_window_callback_impl(GLFWwindow*, int key, int scan_code, int action, int mode)
{
	auto pressed_key = glfwGetKeyName(key, scan_code);

	// if (glfwGetKey(window.get_window(), key) == GLFW_RELEASE)
	// {
	// 	return; // ignore key releases
	// }

	if (action == GLFW_REPEAT)
	{
		// return; // ignore held keys
	} else {
		printf("input detected [%d], key:=%d, scan_code:=%d, action:=%d, mode:=%d, translated_key:=%s\n", 
			inc++, key, scan_code, action, mode, pressed_key ? pressed_key : "N/A");
	}

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

		case GLFW_KEY_LEFT:
			break;

		case GLFW_KEY_RIGHT:
			break;

		case GLFW_KEY_S:
		{
			if (mode == GLFW_MOD_SHIFT)
			{
				auto& obj = spawn_object<Cube>("../resources/textures/texture.jpg");
				obj.set_position(glm::vec3(1.0f, 0.0f, 0.0f));
			} else {
				auto& obj = spawn_object<Sphere>();
				obj.set_position(glm::vec3(-1.0f, 0.0f, 0.0f));
			}
			break;
		}
		case GLFW_KEY_X: // experimental
		{
			experimental->process();
			break;
		}
		case GLFW_KEY_F: // wireframe mode
		{
			graphics_engine->enqueue_cmd(std::make_unique<ToggleWireFrameModeCmd>());
			break;
		}
		case GLFW_KEY_C: // toggle camera focus visibility
		{
			if (mode == GLFW_MOD_SHIFT)
			{
				camera->toggle_mode();
			} else {
				camera->toggle_visibility();
			}
			break;
		}
		case GLFW_KEY_U: // simulation
		{
			if (simulations.empty())
				simulations.push_back(std::make_unique<TowerOfHanoi>(*this));
			simulations[0]->start();
			break;
		}
		case GLFW_KEY_R:
		{
			if (mode == GLFW_MOD_SHIFT)
			{
				HotReload::get().reload();
			}
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
	if (ImGui::GetIO().WantCaptureMouse)
	{
		return;
	}
	reinterpret_cast<GameEngine*>(glfwGetWindowUserPointer(glfw_window))->handle_mouse_button_callback_impl(glfw_window, button, action, mode);
}

void GameEngine::handle_mouse_button_callback_impl(GLFWwindow* glfw_window, int button, int action, int mode)
{
	if (button == GLFW_MOUSE_BUTTON_RIGHT) {
		if (action == GLFW_PRESS) {
			mouse.rmb_down = true;
			mouse.update_pos();
			camera->update_tracker();
		} else if (action == GLFW_RELEASE) {
			mouse.rmb_down = false; 
		}
	} else if (button == GLFW_MOUSE_BUTTON_LEFT)
	{
		if (action == GLFW_PRESS)
		{
			mouse.update_pos();
			if (mode == GLFW_MOD_SHIFT)
			{
				Maths::Ray ray = screen_to_world(mouse.curr_pos);
				auto simple_collision_detection = [&](const Object& obj)
				{
					// assuming unit box, uses a sphere for efficiency
					float radius = 0.5f; //glm::compMax(obj.get_scale()) / 2.0f;
					glm::vec3 projP = -glm::dot(ray.origin, ray.direction) * ray.direction + 
						ray.origin + glm::dot(ray.direction, obj.get_position()) * ray.direction;
					// std::cout << glm::to_string(ray.origin) << ' ' << glm::to_string(ray.direction) << '\n';
					// std::cout << glm::to_string(projP) << ' ' << glm::distance(projP, obj.get_position()) << '\n';
					return glm::distance(projP, obj.get_position()) < radius;
				};

				for (auto& object : objects)
				{
					if (simple_collision_detection(*object))
					{
						object->attach_to(&gizmo);
						return;
					}
				}

				// no objects detected deselect everything
				gizmo.detach_all_children();
			} else {
				if (gizmo.is_active())
				{
					Maths::Ray ray = screen_to_world(mouse.curr_pos);
					gizmo.check_collision(ray);
				}

				mouse.lmb_down = true;
				mouse.orig_pos = mouse.curr_pos;
				//tracker.update(gizmo); // old method to update an object's position via click dragging
			}
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
			camera->update_tracker();
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
	const float sensitivity = 0.2f;
	camera->zoom_in(sensitivity * yoffset);
}