#include "input.hpp"

#include "GLFW/glfw3.h"

#include <algorithm>


Mouse::Mouse(App::Window& ref_window) :
	window(ref_window)
{
}

glm::vec2 Mouse::update_pos()
{
	prev_pos = curr_pos;

	double x, y;
	glfwGetCursorPos(window.get_window(), &x, &y);
	curr_pos.x = std::clamp( 2.0f * static_cast<float>(x) / window.get_width()  - 1.0f, -1.0f, 1.0f);
	// glfw has top->down as negative but our coordinate system is Y+ up
	curr_pos.y = std::clamp(-2.0f * static_cast<float>(y) / window.get_height() + 1.0f, -1.0f, 1.0f);
	return curr_pos;
}

glm::vec2 Mouse::get_orig_offset()
{
	return curr_pos - orig_pos;
}

glm::vec2 Mouse::get_prev_offset()
{
	return curr_pos - prev_pos;
}