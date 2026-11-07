#pragma once

#include "iwindow_callbacks.hpp"
#include "window_presentation_state.hpp"

#include <glm/vec2.hpp>

#include <memory>
#include <functional>
#include <atomic>
#include <cstdint>


struct GLFWwindow;

namespace App {
	class Window
	{
	public:
		Window();
		Window(int width, int height);
		virtual ~Window();

		virtual void setup_callbacks(IWindowCallbacks& callbacks);
		virtual void poll_events();
		// Initialization finishes before the window becomes interactive.
		virtual void show();
		virtual bool should_close();
		int get_width();
		int get_height();
		virtual PresentationState get_presentation_state() const;
		bool wait_for_presentation_change(std::uint64_t generation, const std::atomic<bool>& shutdown) const;
		void wake_presentation_waiters() const;

		// returns position of cursor relative to window [-1:1]
		// top left corner = {x:-1, y:1}
		virtual glm::vec2 get_cursor_pos();
		virtual void set_cursor_captured(bool captured);
		bool is_cursor_captured() const { return cursor_captured; }

		GLFWwindow* get_glfw_window() { return window; }

	protected:
		explicit Window(bool create_window);

	private:
		static void framebuffer_size_callback(GLFWwindow* window, int width, int height);
		static void iconify_callback(GLFWwindow* window, int iconified);
		static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
		static void key_callback(GLFWwindow* window, int key, int scan_code, int action, int mode);
		static void mouse_button_callback(GLFWwindow* window, int button, int action, int mode);
		void publish_presentation_state();
		void publish_presentation_state(int framebuffer_width, int framebuffer_height, bool iconified);

		Window(bool create_window, int width, int height);
		static constexpr int INITIAL_WINDOW_WIDTH = 1400;
		static constexpr int INITIAL_WINDOW_HEIGHT = 800;
		GLFWwindow* window = nullptr;
		bool cursor_captured = false;
		IWindowCallbacks* callbacks = nullptr;
		WindowPresentationState presentation_state;
	};
}
