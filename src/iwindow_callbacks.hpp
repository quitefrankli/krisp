#pragma once


class IWindowCallbacks
{
public:
	virtual void scroll_callback(double yoffset) = 0;
	virtual void key_callback(int key, int scan_code, int action, int mode) = 0;
	virtual void mouse_button_callback(int button, int action, int mode, bool gui_wants_input) = 0;
};