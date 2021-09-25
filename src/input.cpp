#include "input.hpp"

#include "GLFW/glfw3.h"

#include <algorithm>


bool Input::operator!=(const Pos& p1, const Pos& p2)
{
	return p1.x != p2.x || p1.y != p2.y;
}

Mouse::Mouse(App::Window& ref_window) :
	window(ref_window)
{
}

void Mouse::update_pos()
{
	previous_pos = current_pos;

	glfwGetCursorPos(window.get_window(), &current_pos.x, &current_pos.y);
	// normalise coordinates
	current_pos.x = -1.0 + 2.0 * (double)current_pos.x / window.get_width();
	current_pos.y = 1.0 - 2.0 * (double)current_pos.y / window.get_height();
	current_pos.x = std::clamp(current_pos.x, -1.0, 1.0);
	current_pos.y = std::clamp(current_pos.y, -1.0, 1.0);
}
