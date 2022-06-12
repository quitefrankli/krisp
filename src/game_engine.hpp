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
#include "audio_engine/audio_engine_pimpl.hpp"

#include <chrono>
#include <atomic>
#include <thread>
#include <unordered_map>


class GLFWWindow;
class Shape;
class Camera;
template<typename GameEngineT>
class GuiManager;
template<typename GameEngineT>
class Experimental;
template<typename GameEngineT>
class Gizmo;
class IApplication;
class LightSource;
class Analytics;


template<template<typename> typename GraphicsEngineTemplate>
class GameEngine
{
public:
	using GraphicsEngineT = GraphicsEngineTemplate<GameEngine>;

public: // getters and setters
	Camera& get_camera() { return *camera; }
	GLFWwindow* get_window() { return window.get_window(); }
	GraphicsEngineT& get_graphics_engine() { return *graphics_engine; }
	GuiManager<GameEngine>& get_gui_manager();
	AudioEnginePimpl& get_audio_engine() { return audio_engine; }

public:
	GameEngine(std::function<void()>&& restart_signaller);
	~GameEngine();

	void run();
	void main_loop(const float time_delta);
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

	void delete_object(uint64_t id);
	void highlight_object(const Object& object);
	void unhighlight_object(const Object& object);
	void restart();

	void set_application(IApplication* application) { this->application=application; }

	LightSource* get_light_source() { return light_source; }

	static void set_global_engine(GameEngine* global_engine)
	{
		GameEngine::global_engine = global_engine;
	}

	static GameEngine& get()
	{
		return *global_engine;
	}

	float get_tps() const { return tps; }
	void set_tps(const float tps) { this->tps = tps; }
	uint32_t get_window_width();
	uint32_t get_window_height();

private:
	App::Window<GameEngine> window;
	AudioEnginePimpl audio_engine;
    std::unique_ptr<GraphicsEngineT> graphics_engine;
	std::unique_ptr<Camera> camera;
	std::unique_ptr<Gizmo<GameEngine>> gizmo;
	Keyboard keyboard;
	std::unique_ptr<Mouse<GameEngine>> mouse;
	ResourceLoader resource_loader;

	std::atomic<bool> should_shutdown = false;
	std::unordered_map<uint64_t, std::shared_ptr<Object>> objects;
	std::thread graphics_engine_thread;
	std::unique_ptr<Experimental<GameEngine>> experimental;
	LightSource* light_source = nullptr;
	IApplication* application = nullptr;

	static GameEngine* global_engine;

public:
	Animator animator;

private:
	void create_camera();
	void shutdown_impl();
	const std::function<void()> restart_signaller;
	std::unique_ptr<Analytics> TPS_counter;
	float tps;

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
	friend Experimental<GameEngine>;
};