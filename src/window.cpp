#pragma once

#include "window.hpp"
#include "game_engine.hpp"
#include "camera.hpp"
#include "graphics_engine/graphics_engine.hpp"

#include <GLFW/glfw3.h>

#include <cassert>


void App::Window::init(void* callback_ptr, GLFWscrollfun scroll_cb, GLFWkeyfun key_cb, GLFWmousebuttonfun mouse_button_cb)
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	window = std::unique_ptr<GLFWwindow, std::function<void(GLFWwindow*)>>(
		glfwCreateWindow(INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT, "Vulkan", nullptr, nullptr),
		[](GLFWwindow* _window)
		{
			glfwDestroyWindow(_window);
		});

	const auto* monitor = glfwGetVideoMode(glfwGetPrimaryMonitor());
	glfwSetWindowPos(get_window(), (monitor->width - INITIAL_WINDOW_WIDTH)/2, 50);
	// glfwSetFramebufferSizeCallback(get_window(), GameEngineT::handle_window_resize_callback);
	// glfwCreateWindow(WIDTH, HEIGHT, "GUI", nullptr, nullptr); // it's possible to have multiple windows

	glfwSetWindowUserPointer(get_window(), callback_ptr);
	glfwSetScrollCallback(get_window(), scroll_cb);
	glfwSetKeyCallback(get_window(), key_cb);
	glfwSetMouseButtonCallback(get_window(), mouse_button_cb);
}

int App::Window::get_width()
{
	if (!window)
	{
		return INITIAL_WINDOW_WIDTH;
	}

	int width, height;
	glfwGetWindowSize(get_window(), &width, &height);
	return width;
}

int App::Window::get_height()
{
	if (!window)
	{
		return INITIAL_WINDOW_HEIGHT;
	}

	int width, height;
	glfwGetWindowSize(get_window(), &width, &height);
	return height;
}

App::Window::~Window()
{
	if (!window)
	{
		return;
	}

	glfwDestroyWindow(get_window());
	glfwTerminate();
}

GLFWwindow* App::Window::get_window()
{
	assert(window);
	return window.get();
}