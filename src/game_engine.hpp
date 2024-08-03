#pragma once

#include "input.hpp"
#include "objects/object.hpp"
#include "objects/objects.hpp"
#include "objects/object_interfaces/clickable.hpp"
#include "resource_loader.hpp"
#include "maths.hpp"
#include "gui/gui_manager.hpp"
#include "graphics_engine/graphics_engine_commands.hpp"
#include "audio_engine/audio_engine_pimpl.hpp"
#include "window.hpp"
#include "entity_component_system/ecs.hpp"
#include "entity_deletion_queue.hpp"
#include "graphics_engine/engine_base.hpp"

#include <chrono>
#include <atomic>
#include <thread>
#include <unordered_map>
#include <queue>


class Camera;
class GuiManager;
class Experimental;
class Gizmo;
class IApplication;
class Analytics;


class GameEngine : public IWindowCallbacks
{
public: // getters and setters
	Camera& get_camera() { return *camera; }
	App::Window& get_window() { return window; }
	GraphicsEngineBase& get_graphics_engine() { return *graphics_engine; }
	GuiManager& get_gui_manager();
	AudioEnginePimpl& get_audio_engine() { return audio_engine; }
	Gizmo& get_gizmo();

	using GraphicsEngineFactory = std::function<std::unique_ptr<GraphicsEngineBase>(GameEngine&)>;

public:
	GameEngine(App::Window& window);
	GameEngine(App::Window& window, GraphicsEngineFactory factory);
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
		Object& new_obj = *(result.first->second);
		ecs.add_object(new_obj);
		send_graphics_cmd(std::make_unique<SpawnObjectCmd>(result.first->second));
		return static_cast<object_t&>(new_obj);
	}

	Object& spawn_object(std::shared_ptr<Object>&& object);

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

	void delete_object(ObjectID id);
	void highlight_object(const Object& object);
	void unhighlight_object(const Object& object);

	void set_application(IApplication* application) { this->application=application; }

	ECS& get_ecs() { return ecs; }
	const ECS& get_ecs() const { return ecs; }

	float get_tps() const { return tps; }
	void set_tps(const float tps) { this->tps = tps; }
	uint32_t get_window_width();
	uint32_t get_window_height();
	Maths::Ray get_mouse_ray() const;

	Object* get_object(ObjectID id)
	{
		auto it = objects.find(id);
		return it == objects.end() ? nullptr : it->second.get();
	}
	std::unordered_map<ObjectID, std::shared_ptr<Object>>& get_objects() { return objects; }

	void preview_objs_in_gui(const std::vector<Object*>& objs, GuiPhotoBase& gui_window);

private:
	App::Window& window;
	AudioEnginePimpl audio_engine;
    std::unique_ptr<GraphicsEngineBase> graphics_engine;
	std::unique_ptr<Camera> camera;
	std::unique_ptr<Gizmo> gizmo;
	Keyboard keyboard;
	std::unique_ptr<Mouse> mouse;
	ResourceLoader resource_loader;
	ECS& ecs;

	std::atomic<bool> should_shutdown = false;
	std::unordered_map<ObjectID, std::shared_ptr<Object>> objects;
	std::thread graphics_engine_thread;
	IApplication* application = nullptr;

	std::unique_ptr<Experimental> experimental;
	EntityDeletionQueue entity_deletion_queue;	

private:
	void shutdown_impl();
	void process_objs_to_delete();
	std::unique_ptr<Analytics> TPS_counter;
	float tps;

public: // callbacks
	virtual void scroll_callback(double yoffset) override;
	virtual void key_callback(int key, int scan_code, int action, int mode) override;
	virtual void mouse_button_callback(int button, int action, int mode, bool gui_wants_input) override;
	// void pause();
	
private: // friends
	friend Experimental;
};