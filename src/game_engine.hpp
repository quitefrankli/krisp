#pragma once

#include "graphics_engine.hpp"
#include "input.hpp"
#include "camera.hpp"
#include "window.hpp"

#include <atomic>


class GLFWWindow;

class GameEngine
{
private:
	App::Window window;
    GraphicsEngine graphics_engine;
	Keyboard keyboard;
	Mouse mouse;
	Camera camera;
	std::atomic<bool> should_shutdown = false;

private:
	void create_camera();
	void shutdown_impl();
	void run_impl();
	
public: // getters and setters
	Camera& get_camera() { return camera; }
	GLFWwindow* get_window() { return window.get_window(); }

public:
	GameEngine();
	~GameEngine() {}

	void run();
	void shutdown() { shutdown_impl(); }


public: // callbacks
	static void handle_window_callback(GLFWwindow* glfw_window, int key, int scan_code, int action, int mode);
	static void handle_window_resize_callback(GLFWwindow* glfw_window, int width, int height);

private: // callbacks
	void handle_window_callback_impl(GLFWwindow* glfw_window, int key, int scan_code, int action, int mode);
	void handle_window_resize_callback_impl(GLFWwindow* glfw_window, int width, int height);

	// void pause();
};