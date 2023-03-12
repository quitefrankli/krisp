#pragma once

#include "iwindow_callbacks.hpp"

#include <glm/vec2.hpp>

#include <memory>
#include <functional>


struct GLFWwindow;

namespace App {
	class Window
	{
	public:
		Window();
		~Window();

		void setup_callbacks(IWindowCallbacks& callbacks);
		void poll_events();
		bool should_close();
		int get_width();
		int get_height();

		// returns position of cursor relative to window [-1:1]
		// top left corner = {x:-1, y:1}
		glm::vec2 get_cursor_pos();

		GLFWwindow* get_glfw_window() { return window; }

	private:
		const int INITIAL_WINDOW_WIDTH = 1400;
		const int INITIAL_WINDOW_HEIGHT = 800;
		GLFWwindow* window = nullptr;
	};
}