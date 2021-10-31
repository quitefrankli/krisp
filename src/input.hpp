#pragma once

#include "window.hpp"

#include <glm/vec2.hpp>


class Keyboard
{

};

namespace Input
{
	struct Pos
	{
		double x = 0;
		double y = 0;
	};

	bool operator!=(const Pos& p1, const Pos& p2);
}

class Mouse
{
public:
	Mouse(App::Window& window);

	bool is_different_pos() { return current_pos != previous_pos; }

	Input::Pos click_drag_orig_pos;
	Input::Pos current_pos;
	bool rmb_down = false;
	bool lmb_down = false;
	bool mmb_down = false;

	void update_pos();

	glm::vec2 get_pos();

private:
	App::Window& window;
	Input::Pos previous_pos;
};


