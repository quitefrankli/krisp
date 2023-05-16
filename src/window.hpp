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
		Window() = default;
		~Window();

		virtual void open(int x0, int y0);
		virtual void setup_callbacks(IWindowCallbacks& callbacks);
		virtual void poll_events();
		virtual bool should_close();
		int get_width();
		int get_height();

		// returns position of cursor relative to window [-1:1]
		// top left corner = {x:-1, y:1}
		virtual glm::vec2 get_cursor_pos();

		GLFWwindow* get_glfw_window() { return window; }

		bool is_shift_down() const { return shift_down; }
		void set_shift_down(bool shift_down) { this->shift_down = shift_down; }

	private:
		const int INITIAL_WINDOW_WIDTH = 1400;
		const int INITIAL_WINDOW_HEIGHT = 800;
		GLFWwindow* window = nullptr;

		bool shift_down = false;
	};
}