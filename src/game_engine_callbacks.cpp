#pragma once

#include "game_engine.hpp"

#include "camera.hpp"
#include "objects/objects.hpp"
#include "graphics_engine/graphics_engine.hpp"
#include "graphics_engine/graphics_engine_commands.hpp"
#include "analytics.hpp"
// #include "hot_reload.hpp"
#include "experimental.hpp"
#include "iapplication.hpp"
#include "interface/gizmo.hpp"


#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <quill/LogMacros.h>

#include <iostream>
#include <vector>
#include <thread>
#include <chrono>


void GameEngine::key_callback(const KeyInput& key_input)
{
	using enum EKeyModifier;
	using enum EInputAction;

	if (key_input.eq(GLFW_KEY_ESCAPE, NONE, PRESS))
		shutdown();
	else if (key_input.eq(GLFW_KEY_X, NONE, PRESS))
		experimental->process();
	else if (key_input.eq(GLFW_KEY_R, SHIFT, PRESS))
	{
		// HotReload::get().reload();
	}
	else if (key_input.eq(GLFW_KEY_BACKSPACE, NONE, PRESS) ||
			 key_input.eq(GLFW_KEY_DELETE, NONE, PRESS))
		gizmo->delete_object();
	else if (key_input.eq(GLFW_KEY_LEFT_SUPER, NONE, PRESS)) // for macbook dragging
	{
		mouse->mmb_down = true;
		mouse->update_pos();
		mouse->orig_pos = mouse->curr_pos;
		camera->update_tracker();
	}
	else if (key_input.eq(GLFW_KEY_LEFT_SUPER, NONE, RELEASE))
		mouse->mmb_down = false;
		
	keyboard.update_key(key_input);
	application->on_key_press(*this, key_input);
}

void GameEngine::mouse_button_callback(const MouseInput& mouse_input, bool gui_wants_input)
{
	using enum EMouseButton;
	using enum EInputAction;
	using enum EKeyModifier;

	if (gui_wants_input)
	{
		if (mouse_input.action != RELEASE)
		{
			return;
		}

		if (!(mouse->lmb_down || mouse->rmb_down || mouse->mmb_down))
		{
			return;
		}
	}

	if (mouse_input.eq(RIGHT, NONE, PRESS))
	{
		mouse->rmb_down = true;
		mouse->update_pos();
		camera->update_tracker();
	} else if (mouse_input.eq(RIGHT, NONE, RELEASE)) {
		mouse->rmb_down = false; 
	} else if (mouse_input.eq(LEFT, NONE, PRESS)) {
		mouse->update_pos();
		if (gizmo->is_active())
		{
			const Maths::Ray ray = get_mouse_ray();
			gizmo->check_collision(ray);
		}

		mouse->lmb_down = true;
		mouse->orig_pos = mouse->curr_pos;
	} else if (mouse_input.eq(LEFT, SHIFT, PRESS)) {
		mouse->update_pos();
		const Maths::Ray ray = camera->get_ray(mouse->curr_pos);
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
	} else if (mouse_input.eq(LEFT, NONE, RELEASE)) {
		mouse->lmb_down = false; 
	} else if (mouse_input.eq(MIDDLE, NONE, PRESS)) {
		mouse->mmb_down = true;
		mouse->update_pos();
		mouse->orig_pos = mouse->curr_pos;
		camera->update_tracker();
	} else if (mouse_input.eq(MIDDLE, NONE, RELEASE)) {
		mouse->mmb_down = false; 
	}
}

void GameEngine::scroll_callback(double yoffset)
{
	camera->zoom_in(yoffset);
	static const glm::vec3 scale_factor(0.238f);
	gizmo->set_scale(scale_factor * camera->get_focal_length());
}

