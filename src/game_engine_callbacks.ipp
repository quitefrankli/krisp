#pragma once

#include "game_engine.hpp"

#include "camera.hpp"
#include "objects/objects.hpp"
#include "graphics_engine/graphics_engine.hpp"
#include "graphics_engine/graphics_engine_commands.hpp"
#include "utility_functions.hpp"
#include "analytics.hpp"
#include "hot_reload.hpp"
#include "experimental.hpp"
#include "iapplication.hpp"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <quill/Quill.h>
#include <imgui.h>

#include <iostream>
#include <vector>
#include <thread>
#include <chrono>


template<template<typename> typename GraphicsEngineTemplate>
void GameEngine<GraphicsEngineTemplate>::handle_window_callback(GLFWwindow* glfw_window, int key, int scan_code, int action, int mode)
{
	if (ImGui::GetIO().WantCaptureKeyboard)
	{
		return;
	}

	try
	{
		GameEngine* engine = static_cast<GameEngine*>(glfwGetWindowUserPointer(glfw_window));
		engine->handle_window_callback_impl(glfw_window, key, scan_code, action, mode);
	} catch (const std::exception& e)
	{
		std::cerr << "Exception: " << e.what() << '\n';
		throw e;
	}
}

static int inc = 0;

template<template<typename> typename GraphicsEngineTemplate>
void GameEngine<GraphicsEngineTemplate>::handle_window_callback_impl(GLFWwindow*, int key, int scan_code, int action, int mode)
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
				auto& obj = spawn_object<Cube>();
				obj.set_position(glm::vec3(1.0f, 0.3f, 0.0f));
			} else {
				auto& obj = spawn_object<Sphere>();
				obj.set_position(glm::vec3(-1.0f, -0.3f, 0.0f));
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
		case GLFW_KEY_R:
		{
			switch (mode) {
			case GLFW_MOD_SHIFT:
				HotReload::get().reload();break;
			case GLFW_MOD_SHIFT | GLFW_MOD_CONTROL:
				restart();break;
			default:
				break;
			}
		}
		case GLFW_KEY_BACKSPACE:
		case GLFW_KEY_DELETE:
		{
			gizmo->delete_object();
			break;
		}
		default:
			break;
	}
}

template<template<typename> typename GraphicsEngineTemplate>
void GameEngine<GraphicsEngineTemplate>::handle_window_resize_callback(GLFWwindow* glfw_window, int width, int height) {
	reinterpret_cast<GameEngine*>(glfwGetWindowUserPointer(glfw_window))->handle_window_resize_callback_impl(glfw_window, width, height);
}

template<template<typename> typename GraphicsEngineTemplate>
void GameEngine<GraphicsEngineTemplate>::handle_window_resize_callback_impl(GLFWwindow* glfw_window, int width, int height)
{
	// graphics_engine->set_frame_buffer_resized();
	// TODO handle resizing of window
}

template<template<typename> typename GraphicsEngineTemplate>
void GameEngine<GraphicsEngineTemplate>::handle_mouse_button_callback(GLFWwindow* glfw_window, int button, int action, int mode)
{
	if (ImGui::GetIO().WantCaptureMouse)
	{
		return;
	}

	try
	{
		GameEngine* engine = static_cast<GameEngine*>(glfwGetWindowUserPointer(glfw_window));
		engine->handle_mouse_button_callback_impl(glfw_window, button, action, mode);
	} catch (const std::exception& e)
	{
		std::cerr << "Exception: " << e.what() << '\n';
		throw e;
	}
}

template<template<typename> typename GraphicsEngineTemplate>
void GameEngine<GraphicsEngineTemplate>::handle_mouse_button_callback_impl(GLFWwindow* glfw_window, int button, int action, int mode)
{
	if (button == GLFW_MOUSE_BUTTON_RIGHT) {
		if (action == GLFW_PRESS) {
			mouse->rmb_down = true;
			mouse->update_pos();
			camera->update_tracker();
		} else if (action == GLFW_RELEASE) {
			mouse->rmb_down = false; 
		}
	} else if (button == GLFW_MOUSE_BUTTON_LEFT)
	{
		if (action == GLFW_PRESS)
		{
			mouse->update_pos();
			const Maths::Ray ray = camera->get_ray(mouse->curr_pos);
			glm::vec3 intersection{};
			if (mode == GLFW_MOD_SHIFT)
			{
				// gizmo mode, TODO: make else case also a IBaseDispatcher
				OnClickDispatchers::IBaseDispatcher& on_click_dispatcher = *gizmo;
				IClickable* closest_clickable = nullptr;
				float closest_clickable_distance = std::numeric_limits<float>::infinity();
				execute_on_clickables([&closest_clickable, &closest_clickable_distance, &ray](IClickable* clickable)
				{
					glm::vec3 intersection;
					if (clickable->check_click(ray, intersection))
					{
						const float new_distance = glm::distance2(ray.origin, intersection);
						if (new_distance < closest_clickable_distance)
						{
							closest_clickable_distance = new_distance;
							closest_clickable = clickable;
						}
					}
					
					return true;
				});

				// no objects detected deselect everything
				if (closest_clickable)
				{
					closest_clickable->on_click(on_click_dispatcher, ray, intersection);
				} else
				{
					gizmo->deselect();
				}
			} else {
				if (gizmo->is_active())
				{
					gizmo->check_collision(ray);
				} else {
					// TODO: Add this back, but maybe we don't alert the "application" that a click occurred

					// Object* closest_object = nullptr;
					// float closest_distance = std::numeric_limits<float>::infinity();
					// for (auto& obj_pair : objects)
					// {
					// 	glm::vec3 intersection;
					// 	if (obj_pair.second->check_collision(ray, intersection))
					// 	{
					// 		// for debugging purposes TODO: add GuiWindow for debugging
					// 		// spawn_object<Sphere>().set_position(intersection);
					// 		const float new_distance  = glm::distance2(ray.origin, intersection);
					// 		if (new_distance < closest_distance)
					// 		{
					// 			closest_object = obj_pair.second.get();
					// 			closest_distance = new_distance;
					// 		}
					// 	}
					// }

					// if (closest_object)
					// {
					// 	application->on_click(*closest_object);
					// }
				}

				mouse->lmb_down = true;
				mouse->orig_pos = mouse->curr_pos;
				//tracker.update(gizmo); // old method to update an object's position via click dragging
			}
		} else if (action == GLFW_RELEASE)
		{
			mouse->lmb_down = false;
		}
	} else if (button == GLFW_MOUSE_BUTTON_MIDDLE)
	{
		if (action == GLFW_PRESS)
		{
			mouse->mmb_down = true;
			mouse->update_pos();
			mouse->orig_pos = mouse->curr_pos;
			camera->update_tracker();
		} else if (action == GLFW_RELEASE)
		{
			mouse->mmb_down = false;
		}
	}
}

template<template<typename> typename GraphicsEngineTemplate>
void GameEngine<GraphicsEngineTemplate>::handle_scroll_callback(GLFWwindow* glfw_window, double, double yoffset)
{
	reinterpret_cast<GameEngine*>(glfwGetWindowUserPointer(glfw_window))->handle_scroll_callback_impl(glfw_window, yoffset);
}

template<template<typename> typename GraphicsEngineTemplate>
void GameEngine<GraphicsEngineTemplate>::handle_scroll_callback_impl(GLFWwindow* glfw_window, double yoffset)
{
	camera->zoom_in(yoffset);
}