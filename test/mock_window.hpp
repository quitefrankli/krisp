#include <window.hpp>


class MockWindow : public App::Window
{
public:
	virtual void init(void* callback_ptr, GLFWscrollfun scroll_cb, GLFWkeyfun key_cb, GLFWmousebuttonfun mouse_button_cb) override
	{
	}
};