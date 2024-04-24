#pragma once

#include "window.hpp"
#include "game_engine.hpp"
#include "camera.hpp"
#include "graphics_engine/graphics_engine.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <ImGui/imgui_impl_glfw.h>
#include <fmt/core.h>

#include <cassert>


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

	try
	{
		auto* window_callback = static_cast<IWindowCallbacks*>(glfwGetWindowUserPointer(window));
		window_callback->key_callback(key, scan_code, action, mode);
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
		return;
	}

	try
	{
		auto* window_callback = static_cast<IWindowCallbacks*>(glfwGetWindowUserPointer(window));
		window_callback->mouse_button_callback(button, action, mode);
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
	window = glfwCreateWindow(INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT, "Vulkan", nullptr, nullptr);
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