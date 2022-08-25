#pragma once

#include "window.hpp"

#include <glm/vec2.hpp>


class Keyboard
{

};

template<typename GameEngineT>
class Mouse
{
public:
	Mouse(App::Window& window);

	glm::vec2 orig_pos; // click drag pos
	glm::vec2 prev_pos; // time delta pos
	glm::vec2 curr_pos;
	bool rmb_down = false;
	bool lmb_down = false;
	bool mmb_down = false;

	bool update_pos_on_significant_offset(const float min_offset);
	glm::vec2 update_pos();
	glm::vec2 get_prev_offset();
	glm::vec2 get_orig_offset();
	
private:
	App::Window& window;
};


