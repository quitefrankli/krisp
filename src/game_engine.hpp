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
class IApplication;
class LightSource;


class GameEngine
{
public: // getters and setters
	Camera& get_camera() { return *camera; }
	GLFWwindow* get_window() { return window.get_window(); }
	GraphicsEngine& get_graphics_engine() { return *graphics_engine; }
	GuiManager& get_gui_manager();

public:
	GameEngine(std::function<void()>&& restart_signaller);
	~GameEngine();

	void run();
	void shutdown() { shutdown_impl(); }
	template<typename object_t, typename... Args>
	object_t& spawn_object(Args&&... args)
	{
		auto tmp_new_obj = std::make_shared<object_t>(std::forward<Args>(args)...);
		auto id = tmp_new_obj->get_id();
		auto result = objects.emplace(id, std::move(tmp_new_obj));
		assert(result.second); // no duplicate ids
		send_graphics_cmd(std::make_unique<SpawnObjectCmd>(result.first->second));
		return static_cast<object_t&>(*(result.first->second));
	}

	void send_graphics_cmd(std::unique_ptr<GraphicsEngineCommand>&& cmd);
	
	// this function assumes something else manages the lifetime of object
	template<typename object_t>
	void draw_object(const std::shared_ptr<object_t>& object)
	{
		send_graphics_cmd(std::make_unique<SpawnObjectCmd>(object));
	}

	template<typename object_t>
	void draw_object(object_t& object)
	{
		send_graphics_cmd(std::make_unique<SpawnObjectCmd>(object));
	}

	void delete_object(obj_id_t id);

	void restart();

	void set_application(IApplication* application) { this->application=application; }

	LightSource* get_light_source() { return light_source; }

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
	std::unordered_map<obj_id_t, std::shared_ptr<Object>> objects;
	std::thread graphics_engine_thread;
	std::unique_ptr<Experimental> experimental;
	LightSource* light_source = nullptr;
	IApplication* application = nullptr;

public:
	Animator animator;
	std::vector<std::unique_ptr<Simulation>> simulations;

private:
	void create_camera();
	void shutdown_impl();
	const std::function<void()> restart_signaller;

public: // callbacks
	static void handle_window_callback(GLFWwindow* glfw_window, int key, int scan_code, int action, int mode);
	static void handle_window_resize_callback(GLFWwindow* glfw_window, int width, int height);
	static void handle_mouse_button_callback(GLFWwindow* glfw_window, int button, int action, int mode);
	static void handle_scroll_callback(GLFWwindow* glfw_window, double xoffset, double yoffset);

private: // callbacks
	void handle_window_callback_impl(GLFWwindow* glfw_window, int key, int scan_code, int action, int mode);
	void handle_window_resize_callback_impl(GLFWwindow* glfw_window, int width, int height);
	void handle_mouse_button_callback_impl(GLFWwindow* glfw_window, int button, int action, int mode);
	void handle_scroll_callback_impl(GLFWwindow* glfw_window, double yoffset);

	// void pause();
	
private: // friends
	friend Experimental;
};