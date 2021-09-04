#pragma once

#include <memory>
#include <functional>

struct GLFWwindow;
class GameEngine;

namespace App {
	class Window
	{
	private:
		const int INITIAL_WINDOW_WIDTH = 800;
		const int INITIAL_WINDOW_HEIGHT = 600;
		std::unique_ptr<GLFWwindow, std::function<void(GLFWwindow*)>> window;

	public:
		Window() = delete;
		Window(GameEngine* game_engine);
		~Window();

		GLFWwindow* get_window();
	};
}