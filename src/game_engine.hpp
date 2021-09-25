#pragma once

#include "input.hpp"
#include "window.hpp"
#include "objects.hpp"
#include <atomic>


class GLFWWindow;
class Shape;
class GraphicsEngine;
class Camera;

class GameEngine
{
private:
	App::Window window;
    std::unique_ptr<GraphicsEngine> graphics_engine;
	std::unique_ptr<Camera> camera;
	Keyboard keyboard;
	Mouse mouse;
	std::atomic<bool> should_shutdown = false;

	std::vector<Object> objects;
private:
	void create_camera();
	void shutdown_impl();
	
public: // getters and setters
	Camera& get_camera() { return *camera; }
	GLFWwindow* get_window() { return window.get_window(); }
	const GraphicsEngine& get_graphics_engine() const { return *graphics_engine; }

public:
	GameEngine();
	~GameEngine();

	void run();
	void shutdown() { shutdown_impl(); }


public: // callbacks
	static void handle_window_callback(GLFWwindow* glfw_window, int key, int scan_code, int action, int mode);
	static void handle_window_resize_callback(GLFWwindow* glfw_window, int width, int height);
	static void handle_mouse_button_callback(GLFWwindow* glfw_window, int button, int action, int mode);
	static void handle_scroll_callback(GLFWwindow* glfw_window, double xoffset, double yoffset);

private: // callbacks
	void handle_window_callback_impl(GLFWwindow* glfw_window, int key, int scan_code, int action, int mode);
	void handle_window_resize_callback_impl(GLFWwindow* glfw_window, int width, int height);
	void handle_mouse_button_callback_impl(GLFWwindow* glfw_window, int button, int action, int mode);
	void handle_scroll_callback_impl(GLFWwindow* glfw_window, double xoffset, double yoffset);

	// void pause();
};