#pragma once

#include "window.hpp"

class Keyboard
{

};

class Mouse
{
public:
	struct Pos
	{
		double x = 0;
		double y = 0;
	};

	Pos click_drag_orig_pos;

public:
	Mouse(App::Window& window);

	Pos current_pos;
	bool rmb_down = false;
	bool lmb_down = false;

	void update_pos();

private:
	App::Window& window;
};

class Input 
{

};