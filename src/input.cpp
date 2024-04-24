#pragma once

#include "input.hpp"

#include <glm/glm.hpp>

#include <algorithm>


Mouse::Mouse(App::Window& ref_window) :
	window(ref_window)
{
}

glm::vec2 Mouse::update_pos()
{
	prev_pos = curr_pos;
	curr_pos = window.get_cursor_pos();

	return curr_pos;
}

glm::vec2 Mouse::get_orig_offset()
{
	return curr_pos - orig_pos;
}

glm::vec2 Mouse::get_curr_pos() const
{
	return window.get_cursor_pos();
}

glm::vec2 Mouse::get_prev_offset()
{
	return curr_pos - prev_pos;
}

bool Mouse::update_pos_on_significant_offset(const float min_offset)
{
	const auto temp_pos = window.get_cursor_pos();
	if (glm::distance(temp_pos, prev_pos) < min_offset)
	{
		return false;
	}

	prev_pos = curr_pos;
	curr_pos = temp_pos;

	return true;
}