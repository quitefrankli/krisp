#pragma once

#include <memory>
#include <functional>

struct GLFWwindow;

namespace App {
	template<typename GameEngineT>
	class Window
	{
	private:
		const int INITIAL_WINDOW_WIDTH = 1400;
		const int INITIAL_WINDOW_HEIGHT = 800;
		std::unique_ptr<GLFWwindow, std::function<void(GLFWwindow*)>> window;
		GameEngineT* game_engine;

	public:
		Window() = delete;
		Window(GameEngineT* game_engine);
		~Window();

		GLFWwindow* get_window();
		int get_width();
		int get_height();
	};
}