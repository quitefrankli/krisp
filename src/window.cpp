#pragma once

#include "window.hpp"
#include "game_engine.hpp"
#include "camera.hpp"
#include "utility.hpp"

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

static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	if (ImGui::GetIO().WantCaptureMouse)
	{
		ImGui_ImplGlfw_ScrollCallback(window, xoffset, yoffset);
		return;
	}

	try
	{
		auto* window_callback = static_cast<IWindowCallbacks*>(glfwGetWindowUserPointer(window));
		window_callback->scroll_callback(yoffset);
	} catch (const std::exception& e)
	{
		fmt::print("Exception: {}\n", e.what());
		throw e;
	}
}

static void key_callback(GLFWwindow* window, int key, int scan_code, int action, int mode)
{
	if (ImGui::GetIO().WantCaptureKeyboard)
	{
		ImGui_ImplGlfw_KeyCallback(window, key, scan_code, action, mode);
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

		auto* window_callback = static_cast<IWindowCallbacks*>(glfwGetWindowUserPointer(window));
		window_callback->key_callback({key, key_modifier, input_action});
	} catch (const std::exception& e)
	{
		fmt::print("Exception: {}\n", e.what());
		throw e;
	}
}

static void mouse_button_callback(GLFWwindow* window, int button, int action, int mode)
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

		auto* window_callback = static_cast<IWindowCallbacks*>(glfwGetWindowUserPointer(window));
		window_callback->mouse_button_callback({mouse_button, get_key_modifier(mode), input_action}, ImGui::GetIO().WantCaptureMouse);
	} catch (const std::exception& e)
	{
		fmt::print("Exception: {}\n", e.what());
		throw e;
	}
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
	glfwSetWindowUserPointer(window, &callbacks);
	// glfwSetFramebufferSizeCallback(window, IWindowCallbacks::window_resize_callback);
	// glfwSetWindowCloseCallback(window, window_close_callback);
	// glfwSetCursorPosCallback(window, IWindowCallbacks::cursor_pos_callback);
	glfwSetScrollCallback(window, scroll_callback);
	glfwSetKeyCallback(window, key_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);
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
	double pixel_x, pixel_y;
	glfwGetCursorPos(window, &pixel_x, &pixel_y);
	return {
		std::clamp( 2.0f * static_cast<float>(pixel_x) / static_cast<float>(get_width())  - 1.0f, -1.0f, 1.0f),
		// glfw has top->down as negative but our coordinate system is Y+ up
		std::clamp(-2.0f * static_cast<float>(pixel_y) / static_cast<float>(get_height()) + 1.0f, -1.0f, 1.0f)
	};
}

void App::Window::open(int x0, int y0)
{
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
	
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	window = glfwCreateWindow(
		INITIAL_WINDOW_WIDTH, 
		INITIAL_WINDOW_HEIGHT, 
		Utility::get_project_name().data(), 
		nullptr, 
		nullptr);
	assert(window);

	const auto* monitor = glfwGetVideoMode(glfwGetPrimaryMonitor());
	glfwSetWindowPos(window, x0 == -1 ? (monitor->width - INITIAL_WINDOW_WIDTH)/2 : x0, y0 == -1 ? 50 : y0);
	// glfwSetFramebufferSizeCallback(window, GameEngine::handle_window_resize_callback);
	// glfwCreateWindow(WIDTH, HEIGHT, "GUI", nullptr, nullptr); // it's possible to have multiple windows
}

void App::Window::poll_events()
{
	glfwPollEvents();
}

bool App::Window::should_close()
{
	return glfwWindowShouldClose(window);
}