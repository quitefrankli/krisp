#pragma once

#include "input.hpp"
#include "window.hpp"
#include "objects/object.hpp"
#include "objects/objects.hpp"
#include "resource_loader.hpp"
#include "animations/animator.hpp"
#include "maths.hpp"
#include "gui/gui_manager.hpp"
#include "graphics_engine/graphics_engine_commands.hpp"
#include "interface/gizmo.hpp"

#include <chrono>
#include <atomic>
#include <thread>


class GLFWWindow;
class Shape;
class GraphicsEngine;
class Camera;
class Simulation;
class GuiManager;
class Experimental;


class GameEngine
{
public: // getters and setters
	Camera& get_camera() { return *camera; }
	GLFWwindow* get_window() { return window.get_window(); }
	GraphicsEngine& get_graphics_engine() { return *graphics_engine; }
	GuiManager& get_gui_manager();

public:
	GameEngine();
	~GameEngine();

	void run();
	void shutdown() { shutdown_impl(); }
	template<typename Object_T = Object, typename... Args>
	Object_T& spawn_object(Args&&... args)
	{
		auto& object = objects.emplace_back(std::make_shared<Object_T>(std::forward<Args>(args)...));
		graphics_engine->enqueue_cmd(std::make_unique<SpawnObjectCmd>(object));
		return static_cast<Object_T&>(*object);
	}
	
	// this function assumes something else manages the lifetime of object
	template<typename Object_T>
	void draw_object(const std::shared_ptr<Object_T>& object)
	{
		graphics_engine->enqueue_cmd(std::make_unique<SpawnObjectCmd>(object));
	}

	template<typename Object_T>
	void draw_object(Object_T& object)
	{
		graphics_engine->enqueue_cmd(std::make_unique<SpawnObjectCmd>(object));
	}

private:
	App::Window window;
	Gizmo gizmo;
    std::unique_ptr<GraphicsEngine> graphics_engine;
	std::unique_ptr<Camera> camera;
	Keyboard keyboard;
	Mouse mouse;
	ResourceLoader resource_loader;

	std::atomic<bool> should_shutdown = false;
	std::chrono::time_point<std::chrono::system_clock> time;
	std::vector<std::shared_ptr<Object>> objects;
	std::thread graphics_engine_thread;
	std::unique_ptr<Experimental> experimental;

public:
	Animator animator;
	std::vector<std::unique_ptr<Simulation>> simulations;

private:
	void create_camera();
	void shutdown_impl();
	
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
	
private: // friends
	friend Experimental;
};