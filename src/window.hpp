#pragma once

#include <GLFW/glfw3.h>

#include <memory>
#include <functional>

struct GLFWwindow;

namespace App {
	class Window
	{
	private:
		const int INITIAL_WINDOW_WIDTH = 1400;
		const int INITIAL_WINDOW_HEIGHT = 800;
		std::unique_ptr<GLFWwindow, std::function<void(GLFWwindow*)>> window;

	public:
		~Window();

		virtual void init(void* callback_ptr, GLFWscrollfun scroll_cb, GLFWkeyfun key_cb, GLFWmousebuttonfun mouse_button_cb);

		GLFWwindow* get_window();
		int get_width();
		int get_height();
	};
}