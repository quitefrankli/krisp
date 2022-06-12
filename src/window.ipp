#pragma once

#include "window.hpp"
#include "game_engine.hpp"
#include "camera.hpp"
#include "graphics_engine/graphics_engine.hpp"

#include <GLFW/glfw3.h>

template<typename GameEngineT>
App::Window<GameEngineT>::Window(GameEngineT* game_engine)
{
	this->game_engine = game_engine;
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	window = std::unique_ptr<GLFWwindow, std::function<void(GLFWwindow*)>>(
		glfwCreateWindow(INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT, "Vulkan", nullptr, nullptr),
		[](GLFWwindow* _window)
		{
			glfwDestroyWindow(_window);
		});
	glfwSetWindowUserPointer(get_window(), game_engine);

	const auto* monitor = glfwGetVideoMode(glfwGetPrimaryMonitor());
	glfwSetWindowPos(get_window(), (monitor->width - INITIAL_WINDOW_WIDTH)/2, 50);
	glfwSetFramebufferSizeCallback(get_window(), GameEngineT::handle_window_resize_callback);
	// glfwCreateWindow(WIDTH, HEIGHT, "GUI", nullptr, nullptr); // it's possible to have multiple windows
	glfwSetScrollCallback(get_window(), GameEngineT::handle_scroll_callback);

	glfwSetKeyCallback(get_window(), GameEngineT::handle_window_callback);
	glfwSetMouseButtonCallback(get_window(), GameEngineT::handle_mouse_button_callback);
}

template<typename GameEngineT>
int App::Window<GameEngineT>::get_width()
{
	int width, height;
	glfwGetWindowSize(get_window(), &width, &height);
	return width;
}

template<typename GameEngineT>
int App::Window<GameEngineT>::get_height()
{
	int width, height;
	glfwGetWindowSize(get_window(), &width, &height);
	return height;
}

template<typename GameEngineT>
App::Window<GameEngineT>::~Window()
{
	glfwDestroyWindow(get_window());
	glfwTerminate();
}

template<typename GameEngineT>
GLFWwindow* App::Window<GameEngineT>::get_window()
{
	return window.get();
}