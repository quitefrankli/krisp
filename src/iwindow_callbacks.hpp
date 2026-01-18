#pragma once

#include "input.hpp"


class IWindowCallbacks
{
public:
	virtual void scroll_callback(double yoffset) = 0;
	virtual void key_callback(const KeyInput& key_input) = 0;
	virtual void mouse_button_callback(const MouseInput& mouse_input, bool gui_wants_input) = 0;
};