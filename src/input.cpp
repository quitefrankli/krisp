#include "input.hpp"

#include "GLFW/glfw3.h"

#include <algorithm>

Mouse::Mouse(App::Window& ref_window) :
	window(ref_window)
{
}

void Mouse::update_pos()
{
	glfwGetCursorPos(window.get_window(), &current_pos.x, &current_pos.y);
	// normalise coordinates
	current_pos.x = -1.0 + 2.0 * (double)current_pos.x / window.get_width();
	current_pos.y = 1.0 - 2.0 * (double)current_pos.y / window.get_height();
	current_pos.x = std::clamp(current_pos.x, -1.0, 1.0);
	current_pos.y = std::clamp(current_pos.y, -1.0, 1.0);
}
