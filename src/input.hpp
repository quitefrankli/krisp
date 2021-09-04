#pragma once


class Keyboard
{

};

class Mouse
{
public:
	struct Pos
	{
		float x = 0;
		float y = 0;
	};

	Pos click_drag_orig_pos;

private:
	Pos current_pos;
};

class Input 
{

};