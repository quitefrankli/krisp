#pragma once

#include "input.hpp"
#include "objects/object.hpp"
#include "objects/objects.hpp"
#include "objects/object_interfaces/clickable.hpp"
#include "resource_loader.hpp"
#include "game_engine/object_interfaces_menagerie.hpp"
#include "animations/animator.hpp"
#include "maths.hpp"
#include "gui/gui_manager.hpp"
#include "graphics_engine/graphics_engine_commands.hpp"
#include "audio_engine/audio_engine_pimpl.hpp"
#include "window.hpp"

#include <chrono>
#include <atomic>
#include <thread>
#include <unordered_map>


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
class GameEngine : public IWindowCallbacks, public ObjectInterfacesMenagerie
{
public:
	using GraphicsEngineT = GraphicsEngineTemplate<GameEngine>;

public: // getters and setters
	Camera& get_camera() { return *camera; }
	App::Window& get_window() { return window; }
	GraphicsEngineT& get_graphics_engine() { return *graphics_engine; }
	GuiManager<GameEngine>& get_gui_manager();
	AudioEnginePimpl& get_audio_engine() { return audio_engine; }

public:
	GameEngine(std::function<void()>&& restart_signaller, App::Window& window);
	~GameEngine();

	void run();
	void main_loop(const float time_delta);
	void shutdown() { shutdown_impl(); }
	
	void send_graphics_cmd(std::unique_ptr<GraphicsEngineCommand>&& cmd);

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

	Object& spawn_object(std::shared_ptr<Object>&& object)
	{
		auto it = objects.emplace(object->get_id(), std::move(object));
		send_graphics_cmd(std::make_unique<SpawnObjectCmd>(it.first->second));
		return *it.first->second;
	}

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

	Object* get_object(uint64_t id)
	{
		auto it = objects.find(id);
		return it == objects.end() ? nullptr : it->second.get();
	}

	void preview_objs_in_gui(const std::vector<Object*>& objs, GuiPhotoBase& gui_window);

private:
	App::Window& window;
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
	virtual void scroll_callback(double yoffset) override;
	virtual void key_callback(int key, int scan_code, int action, int mode) override;
	virtual void mouse_button_callback(int button, int action, int mode) override;
	// void pause();
	
private: // friends
	friend Experimental<GameEngine>;
};