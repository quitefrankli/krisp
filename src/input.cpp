#pragma once

#include "input.hpp"
#include "window.hpp"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <algorithm>


Mouse::Mouse(App::Window& ref_window) :
	window(&ref_window)
{
}

glm::vec2 Mouse::update_pos()
{
	prev_pos = curr_pos;
	curr_pos = window->get_cursor_pos();

	return curr_pos;
}

glm::vec2 Mouse::get_orig_offset()
{
	return curr_pos - orig_pos;
}

glm::vec2 Mouse::get_curr_pos() const
{
	return window->get_cursor_pos();
}

glm::vec2 Mouse::get_prev_offset()
{
	return curr_pos - prev_pos;
}

bool Mouse::update_pos_on_significant_offset(const float min_offset)
{
	const auto temp_pos = window->get_cursor_pos();
	if (glm::distance(temp_pos, prev_pos) < min_offset)
	{
		return false;
	}

	prev_pos = curr_pos;
	curr_pos = temp_pos;

	return true;
}

void Keyboard::update_key(const KeyInput& key_input)
{
	using enum EInputAction;

	if (key_input.key >= 0 && key_input.key < MAX_KEYS)
	{
		key_states.set(key_input.key, key_input.action == PRESS || key_input.action == REPEAT);
	}
}

bool Keyboard::is_pressed(int key) const
{
	if (key < 0 || key >= MAX_KEYS)
		return false;
	return key_states.test(key);
}

bool Keyboard::w_pressed() const { return is_pressed(GLFW_KEY_W); }
bool Keyboard::s_pressed() const { return is_pressed(GLFW_KEY_S); }
bool Keyboard::a_pressed() const { return is_pressed(GLFW_KEY_A); }
bool Keyboard::d_pressed() const { return is_pressed(GLFW_KEY_D); }
bool Keyboard::q_pressed() const { return is_pressed(GLFW_KEY_Q); }
bool Keyboard::e_pressed() const { return is_pressed(GLFW_KEY_E); }