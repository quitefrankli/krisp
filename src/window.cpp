#include "window.hpp"
#include "game_engine.hpp"

#include <GLFW/glfw3.h>

App::Window::Window(GameEngine* game_engine)
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	window = std::unique_ptr<GLFWwindow, std::function<void(GLFWwindow*)>>(
		glfwCreateWindow(INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT, "Vulkan", nullptr, nullptr),
		[](GLFWwindow* _window)
		{
			glfwDestroyWindow(_window);
		});
	glfwSetWindowUserPointer(game_engine->get_window(), game_engine);
	glfwSetFramebufferSizeCallback(window.get(), GameEngine::handle_window_resize_callback);

	// glfwCreateWindow(WIDTH, HEIGHT, "GUI", nullptr, nullptr); // it's possible to have multiple windows

	glfwSetKeyCallback(get_window(), GameEngine::handle_window_callback);
}

App::Window::~Window()
{
	glfwDestroyWindow(get_window());
	glfwTerminate();
}

GLFWwindow* App::Window::get_window()
{
	return window.get();
}