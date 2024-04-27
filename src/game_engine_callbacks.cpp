#pragma once

#include "game_engine.hpp"

#include "camera.hpp"
#include "objects/objects.hpp"
#include "graphics_engine/graphics_engine.hpp"
#include "graphics_engine/graphics_engine_commands.hpp"
#include "analytics.hpp"
#include "hot_reload.hpp"
#include "experimental.hpp"
#include "iapplication.hpp"
#include "interface/gizmo.hpp"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <quill/Quill.h>
#include <imgui.h>

#include <iostream>
#include <vector>
#include <thread>
#include <chrono>


void GameEngine::key_callback(int key, int scan_code, int action, int mode)
{
	static int inc = 0;
	auto pressed_key = glfwGetKeyName(key, scan_code);

	// if (glfwGetKey(window.get_window(), key) == GLFW_RELEASE)
	// {
	// 	return; // ignore key releases
	// }

	if (action == GLFW_REPEAT)
	{
		// return; // ignore held keys
	} else {
		LOG_INFO(Utility::get().get_logger(), 
				 "input detected [{}], key:={}, scan_code:={}, action:={}, mode={}, translated_key:={}", 
				 inc++, key, scan_code, action, mode, pressed_key ? pressed_key : "N/A");
	}

	switch (key)
	{
		case GLFW_KEY_ESCAPE:
			if (action == GLFW_PRESS)
				shutdown();
			break;

		case GLFW_KEY_LEFT:
		case GLFW_KEY_RIGHT:
		case GLFW_KEY_UP:
		case GLFW_KEY_DOWN:
		case GLFW_KEY_SPACE:
		case GLFW_KEY_Z:
			application->on_key_press(key, scan_code, action, mode);
			break;
		case GLFW_KEY_X: // experimental
		{
			if (action == GLFW_RELEASE)
				break;
			experimental->process();
			break;
		}
		case GLFW_KEY_C: // toggle camera focus visibility
		{
			if (action == GLFW_RELEASE)
				break;
			if (mode == GLFW_MOD_SHIFT)
			{
				camera->toggle_mode();
			} else {
				camera->toggle_visibility();
			}
			break;
		}
		case GLFW_KEY_R:
			if (action == GLFW_RELEASE)
				break;
			switch (mode) {
			case GLFW_MOD_SHIFT:
				HotReload::get().reload();break;
			case GLFW_MOD_SHIFT | GLFW_MOD_CONTROL:
				restart();break;
			default:
				break;
			}
		case GLFW_KEY_BACKSPACE:
		case GLFW_KEY_DELETE:
			if (action == GLFW_RELEASE)
				break;
			gizmo->delete_object();
			break;
		case GLFW_KEY_LEFT_SHIFT:
			if (action == GLFW_PRESS)
			{
				window.set_shift_down(true);
			} else if (action == GLFW_RELEASE)
			{
				window.set_shift_down(false);
			}
			break;
		case GLFW_KEY_LEFT_SUPER: // for macbook dragging
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
			break;
		default:
			break;
	}
}

void GameEngine::mouse_button_callback(int button, int action, int mode)
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
				auto clicked_entity = ecs.check_any_entity_clicked(ray);
				
				if (clicked_entity.bCollided)
				{
					on_click_dispatcher.dispatch_on_click(
						ecs.get_object(clicked_entity.id), 
						ray, 
						clicked_entity.intersection);
				} else
				{
					// no objects detected deselect everything
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

void GameEngine::scroll_callback(double yoffset)
{
	camera->zoom_in(yoffset);
}