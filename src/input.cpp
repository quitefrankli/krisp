#include "input.hpp"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <algorithm>


Mouse::Mouse(App::Window& ref_window) :
	window(ref_window)
{
}

glm::vec2 Mouse::update_pos()
{
	prev_pos = curr_pos;

	double pixel_x, pixel_y;
	glfwGetCursorPos(window.get_window(), &pixel_x, &pixel_y);
	curr_pos.x = std::clamp( 2.0f * static_cast<float>(pixel_x) / window.get_width()  - 1.0f, -1.0f, 1.0f);
	// glfw has top->down as negative but our coordinate system is Y+ up
	curr_pos.y = std::clamp(-2.0f * static_cast<float>(pixel_y) / window.get_height() + 1.0f, -1.0f, 1.0f);
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

bool Mouse::update_pos_on_significant_offset(const float min_offset)
{
	double pixel_x, pixel_y;
	glfwGetCursorPos(window.get_window(), &pixel_x, &pixel_y);

	glm::vec2 temp_pos;
	temp_pos.x = std::clamp( 2.0f * static_cast<float>(pixel_x) / window.get_width()  - 1.0f, -1.0f, 1.0f);
	// glfw has top->down as negative but our coordinate system is Y+ up
	temp_pos.y = std::clamp(-2.0f * static_cast<float>(pixel_y) / window.get_height() + 1.0f, -1.0f, 1.0f);
	if (glm::distance(temp_pos, prev_pos) < min_offset)
	{
		return false;
	}

	prev_pos = curr_pos;
	curr_pos = temp_pos;

	return true;
}