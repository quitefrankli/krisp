#pragma once

#include "window.hpp"
#include "game_engine.hpp"
#include "camera.hpp"
#include "utility.hpp"
#include "config.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <ImGui/imgui_impl_glfw.h>
#include <fmt/core.h>
#include <quill/LogMacros.h>

#include <cassert>


const EKeyModifier get_key_modifier(int mode)
{
	if (mode & GLFW_MOD_CONTROL && mode & GLFW_MOD_SHIFT && mode & GLFW_MOD_ALT)
		return EKeyModifier::CTRL_SHIFT_ALT;
	else if (mode & GLFW_MOD_CONTROL && mode & GLFW_MOD_SHIFT)
		return EKeyModifier::CTRL_SHIFT;
	else if (mode & GLFW_MOD_CONTROL && mode & GLFW_MOD_ALT)
		return EKeyModifier::CTRL_ALT;
	else if (mode & GLFW_MOD_SHIFT && mode & GLFW_MOD_ALT)
		return EKeyModifier::SHIFT_ALT;
	else if (mode & GLFW_MOD_CONTROL)
		return EKeyModifier::CTRL;
	else if (mode & GLFW_MOD_SHIFT)
		return EKeyModifier::SHIFT;
	else if (mode & GLFW_MOD_ALT)
		return EKeyModifier::ALT;
	else
		return EKeyModifier::NONE;
}

const EInputAction get_input_action(int action)
{
	switch (action)
	{
		case GLFW_PRESS: return EInputAction::PRESS;
		case GLFW_RELEASE: return EInputAction::RELEASE;
		case GLFW_REPEAT: return EInputAction::REPEAT;
		default: return EInputAction::RELEASE; // default case
	}
}

void App::Window::scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	const bool gui_wants_input = ImGui::GetIO().WantCaptureMouse;
	if (gui_wants_input)
		ImGui_ImplGlfw_ScrollCallback(window, xoffset, yoffset);

	try
	{
		auto* app_window = static_cast<App::Window*>(glfwGetWindowUserPointer(window));
		app_window->callbacks->scroll_callback(yoffset, gui_wants_input);
	} catch (const std::exception& e)
	{
		fmt::print("Exception: {}\n", e.what());
		throw e;
	}
}

void App::Window::key_callback(GLFWwindow* window, int key, int scan_code, int action, int mode)
{
	// Keep ImGui key state current even after a text field releases capture.
	ImGui_ImplGlfw_KeyCallback(window, key, scan_code, action, mode);
	if (ImGui::GetIO().WantCaptureKeyboard)
	{
		const bool is_function_key = key >= GLFW_KEY_F1 && key <= GLFW_KEY_F25;
		if (!is_function_key)
			return;
	}

	static int _trigger_count = 0;
	try
	{
		const auto pressed_key = glfwGetKeyName(key, scan_code);
		LOG_DEBUG(
			Utility::get_logger(), 
			"input detected [{}], key:={}, scan_code:={}, action:={}, mode={}, translated_key:={}", 
			_trigger_count++, key, scan_code, action, mode, pressed_key ? pressed_key : "N/A");

		const EKeyModifier key_modifier = get_key_modifier(mode);
		const EInputAction input_action = get_input_action(action);

		auto* app_window = static_cast<App::Window*>(glfwGetWindowUserPointer(window));
		app_window->callbacks->key_callback({key, key_modifier, input_action});
	} catch (const std::exception& e)
	{
		fmt::print("Exception: {}\n", e.what());
		throw e;
	}
}

void App::Window::mouse_button_callback(GLFWwindow* window, int button, int action, int mode)
{
	if (ImGui::GetIO().WantCaptureMouse)
	{
		ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mode);
	}

	try
	{
		const EMouseButton mouse_button = [button]() {
			switch (button)
			{
				case GLFW_MOUSE_BUTTON_LEFT: return EMouseButton::LEFT;
				case GLFW_MOUSE_BUTTON_RIGHT: return EMouseButton::RIGHT;
				case GLFW_MOUSE_BUTTON_MIDDLE: return EMouseButton::MIDDLE;
				default: throw std::runtime_error("unsupported mouse button");
			}}();

		const EInputAction input_action = get_input_action(action);

		auto* app_window = static_cast<App::Window*>(glfwGetWindowUserPointer(window));
		app_window->callbacks->mouse_button_callback({mouse_button, get_key_modifier(mode), input_action}, ImGui::GetIO().WantCaptureMouse);
	} catch (const std::exception& e)
	{
		fmt::print("Exception: {}\n", e.what());
		throw e;
	}
}

App::Window::Window() :
	Window(true)
{
}

App::Window::Window(bool create_window) : Window(create_window, INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT) {}

App::Window::Window(int width, int height) : Window(true, width, height) {}

App::Window::Window(bool create_window, int width, int height)
{
	if (!create_window)
	{
		return;
	}

	const int x0 = Config::get_window_pos().first;
	const int y0 = Config::get_window_pos().second;

    // Register error callback first
    glfwSetErrorCallback([](int err_no, const char* err_str)
	{
		const std::vector<int> recoverable_errors =
		{
			65540, // INVALID SCAN CODE
		};

		if (std::find(recoverable_errors.begin(), recoverable_errors.end(), err_no) != recoverable_errors.end())
		{
			fmt::print("App::Window: GLFW Error! errno: {}, reason: '{}'\n", err_no, err_str);
			return;
		}
	});
	
	if (width <= 0 || height <= 0)
		throw std::invalid_argument("Window dimensions must be positive");
	if (!glfwInit())
		throw std::runtime_error("Failed to initialise the window system");

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	window = glfwCreateWindow(
		width,
		height,
		Config::get_project_name().data(), 
		nullptr, 
		nullptr);
	if (!window)
		throw std::runtime_error("Failed to create the engine window");
	glfwSetWindowUserPointer(window, this);
	publish_presentation_state();

	const auto* monitor = glfwGetVideoMode(glfwGetPrimaryMonitor());
	glfwSetWindowPos(window, x0 == -1 && monitor ? (monitor->width - width)/2 : x0, y0 == -1 ? 50 : y0);
	// glfwSetFramebufferSizeCallback(window, GameEngine::handle_window_resize_callback);
	// glfwCreateWindow(WIDTH, HEIGHT, "GUI", nullptr, nullptr); // it's possible to have multiple windows
}

App::Window::~Window()
{
	if (!window)
	{
		return;
	}

	glfwDestroyWindow(window);
	glfwTerminate();
}

void App::Window::setup_callbacks(IWindowCallbacks& callbacks)
{
	this->callbacks = &callbacks;
	glfwSetWindowUserPointer(window, this);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
	glfwSetWindowIconifyCallback(window, iconify_callback);
	// glfwSetFramebufferSizeCallback(window, IWindowCallbacks::window_resize_callback);
	// glfwSetWindowCloseCallback(window, window_close_callback);
	// glfwSetCursorPosCallback(window, IWindowCallbacks::cursor_pos_callback);
	glfwSetScrollCallback(window, scroll_callback);
	glfwSetKeyCallback(window, key_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);
}

void App::Window::framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	auto* app_window = static_cast<App::Window*>(glfwGetWindowUserPointer(window));
	app_window->publish_presentation_state(width, height,
		glfwGetWindowAttrib(window, GLFW_ICONIFIED) == GLFW_TRUE);
}

void App::Window::iconify_callback(GLFWwindow* window, int iconified)
{
	auto* app_window = static_cast<App::Window*>(glfwGetWindowUserPointer(window));
	int framebuffer_width = 0;
	int framebuffer_height = 0;
	glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
	app_window->publish_presentation_state(framebuffer_width, framebuffer_height, iconified == GLFW_TRUE);
}

void App::Window::publish_presentation_state()
{
	if (!window)
		return;
	int framebuffer_width = 0;
	int framebuffer_height = 0;
	glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
	publish_presentation_state(framebuffer_width, framebuffer_height,
		glfwGetWindowAttrib(window, GLFW_ICONIFIED) == GLFW_TRUE);
}

void App::Window::publish_presentation_state(int framebuffer_width, int framebuffer_height, bool iconified)
{
	presentation_state.publish(framebuffer_width, framebuffer_height, iconified);
}

App::PresentationState App::Window::get_presentation_state() const
{
	return presentation_state.get();
}

bool App::Window::wait_for_presentation_change(std::uint64_t generation, const std::atomic<bool>& shutdown) const
{
	return presentation_state.wait_for_change(generation, shutdown);
}

void App::Window::wake_presentation_waiters() const
{
	presentation_state.wake_waiters();
}

int App::Window::get_width()
{
	if (!window)
	{
		return INITIAL_WINDOW_WIDTH;
	}

	int width, height;
	glfwGetWindowSize(window, &width, &height);
	return width;
}

int App::Window::get_height()
{
	if (!window)
	{
		return INITIAL_WINDOW_HEIGHT;
	}

	int width, height;
	glfwGetWindowSize(window, &width, &height);
	return height;
}

glm::vec2 App::Window::get_cursor_pos()
{
	if (!window)
		return glm::vec2(0.0f);
	double pixel_x, pixel_y;
	glfwGetCursorPos(window, &pixel_x, &pixel_y);
	const int logical_width = get_width();
	const int logical_height = get_height();
	if (logical_width <= 0 || logical_height <= 0)
		return glm::vec2(0.0f);
	const glm::vec2 position{
		 2.0f * static_cast<float>(pixel_x) / static_cast<float>(logical_width)  - 1.0f,
		// glfw has top->down as negative but our coordinate system is Y+ up
		-2.0f * static_cast<float>(pixel_y) / static_cast<float>(logical_height) + 1.0f
	};
	return cursor_captured ? position : glm::clamp(position, glm::vec2(-1.0f), glm::vec2(1.0f));
}

void App::Window::set_cursor_captured(const bool captured)
{
	cursor_captured = captured;
	if (window)
		glfwSetInputMode(window, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
}

void App::Window::show()
{
	if (window) glfwShowWindow(window);
}

void App::Window::poll_events()
{
	glfwPollEvents();
}

bool App::Window::should_close()
{
	return glfwWindowShouldClose(window);
}
