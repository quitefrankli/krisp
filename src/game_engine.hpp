#pragma once

#include "input.hpp"
#include "objects/object.hpp"
#include "objects/objects.hpp"
#include "resource_loader/resource_loader.hpp"
#include "maths.hpp"
#include "gui/gui_manager.hpp"
#include "gui/application_ui_manager.hpp"
#include "audio_engine/audio_engine_pimpl.hpp"
#include "window.hpp"
#include "entity_component_system/ecs.hpp"
#include "graphics_engine/engine_base.hpp"
#include "render_frame.hpp"
#include "renderable/render_types.hpp"

#include <atomic>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <stdexcept>
#include <string>


class Camera;
class EngineUiManager;
class Experimental;
class Gizmo;
class IApplication;
class Analytics;
class PlayerCharacter;

enum class EGameMode
{
	EDITOR,
	NORMAL,
};

struct PbrTextureEdit
{
	enum class Action
	{
		Keep,
		Clear,
		Replace,
	};

	Action action = Action::Keep;
	std::string source;
};

struct PbrMaterialEdit
{
	glm::vec4 base_color_factor{ 1.0f };
	float metallic_factor = 1.0f;
	float roughness_factor = 1.0f;
	float normal_scale = 1.0f;
	EAlphaMode alpha_mode = EAlphaMode::OPAQUE;
	float alpha_cutoff = 0.5f;
	bool double_sided = false;
	glm::vec3 emissive_factor{ 0.0f };
	PbrTextureEdit base_color_texture;
	PbrTextureEdit metallic_roughness_texture;
	PbrTextureEdit normal_texture;
	PbrTextureEdit emissive_texture;
};

class GameEngine : public IWindowCallbacks
{
public: // getters and setters
	Camera& get_camera() { return *camera; }
	App::Window& get_window() { return *window; }
	GraphicsEngineBase& get_graphics_engine() { return *graphics_engine; }
	EngineUiManager& get_gui_manager();
	ApplicationUiManager& get_application_ui_manager() { return application_ui_manager; }
	AudioEnginePimpl& get_audio_engine() { return audio_engine; }
	Gizmo& get_gizmo();
	IApplication& get_application() { return *application; }
	const IApplication& get_application() const { return *application; }
	const Keyboard& get_keyboard() const { return keyboard; }
	EGameMode get_game_mode() const { return game_mode; }
	PlayerCharacter* get_active_player() const { return active_player; }
	void set_game_mode(EGameMode mode);
	void toggle_game_mode();
	void set_render_mode(ERenderMode mode);
	void set_exposure_ev(float exposure_ev);
	float get_exposure_ev() const { return render_view_state.exposure_ev; }
	void set_camera_orbit_with_right_mouse(bool enabled) { camera_orbit_with_right_mouse = enabled; }
	void set_free_camera_movement(bool enabled) { free_camera_movement = enabled; }
	void set_normal_mode_cursor_captured(bool captured) { normal_mode_cursor_captured = captured; }

public:
	template<typename AppT, typename... Args>
	static GameEngine create(Args&&... args)
	{
		return GameEngine(std::make_unique<AppT>(std::forward<Args>(args)...));
	}

	~GameEngine();

protected:
	GameEngine(std::unique_ptr<IApplication> application);
	GameEngine(std::unique_ptr<App::Window> window,
			   std::unique_ptr<IApplication> application,
			   std::unique_ptr<GraphicsEngineBase> graphics_engine);

public:
	void run();
	void main_loop(const float time_delta);
	void shutdown() { shutdown_impl(); }
	void reset_scene();
	void save_scene(std::string_view save_name) const;
	void load_scene(std::string_view save_name);

	template<typename object_t, typename... Args>
	object_t& spawn_object(Args&&... args)
	{
		auto tmp_new_obj = std::make_shared<object_t>(std::forward<Args>(args)...);
		auto id = tmp_new_obj->get_id();
		if (objects.contains(id))
			throw std::runtime_error("GameEngine::spawn_object: duplicate object id");
		auto result = objects.emplace(id, std::move(tmp_new_obj));
		Object& new_obj = *(result.first->second);
		ecs.add_object(new_obj);
		return static_cast<object_t&>(new_obj);
	}

	Object& spawn_object(std::shared_ptr<Object>&& object);
	RenderableID attach_renderable(
		ObjectID object_id, Renderable renderable,
		std::optional<SkeletonID> skeleton_id = {});
	std::vector<RenderableID> attach_renderables(
		ObjectID object_id, std::vector<Renderable> renderables,
		std::optional<SkeletonID> skeleton_id = {});
	void spawn_cubemap(std::optional<std::filesystem::path> environment_lighting_asset = {});

	Object& spawn_particle_emitter(const ParticleEmitterConfig& config);

	void delete_object(ObjectID id);
	void highlight_object(const Object& object);
	void unhighlight_object(const Object& object);
	RenderableID set_renderable_pbr_material(
		RenderableID renderable_id,
		glm::vec4 base_color_factor,
		float metallic_factor,
		float roughness_factor);
	RenderableID set_renderable_pbr_material(
		RenderableID renderable_id,
		const PbrMaterialEdit& edit);
	ECS& get_ecs() { return ecs; }
	const ECS& get_ecs() const { return ecs; }

	float get_tps() const { return tps; }
	void set_tps(const float tps) { this->tps = tps; }
	bool is_paused() const { return paused; }
	void set_paused(bool new_paused) { paused = new_paused; }
	void toggle_paused() { paused = !paused; }
	uint32_t get_window_width();
	uint32_t get_window_height();
	Maths::Ray get_mouse_ray() const;

	Object* get_object(ObjectID id)
	{
		auto it = objects.find(id);
		return it == objects.end() ? nullptr : it->second.get();
	}
	std::unordered_map<ObjectID, std::shared_ptr<Object>>& get_objects() { return objects; }
	const std::unordered_map<ObjectID, std::shared_ptr<Object>>& get_objects() const { return objects; }

private:
	std::unique_ptr<App::Window> window;
	AudioEnginePimpl audio_engine;
	ECS ecs;
	std::unordered_map<ObjectID, std::shared_ptr<Object>> objects;
    std::unique_ptr<GraphicsEngineBase> graphics_engine;
	std::unique_ptr<Camera> camera;
	std::unique_ptr<Gizmo> gizmo;
	std::unique_ptr<Mouse> mouse;
	Keyboard keyboard;

	std::atomic<bool> should_shutdown = false;
	std::optional<Renderable> tile_renderable;
	std::thread graphics_engine_thread;
	std::unique_ptr<IApplication> application;
	// Must be destroyed before application and the graphics ImGui context.
	ApplicationUiManager application_ui_manager;

	std::unique_ptr<Experimental> experimental;
	std::queue<ObjectID> entities_to_delete;
	std::unordered_set<ObjectID> pending_deletions;
	std::unordered_map<RenderableID, RenderableDefinitionPtr> renderable_definitions;
	std::unordered_map<SkeletonID, RenderSkeletonDefinitionPtr> render_skeleton_definitions;
	RenderViewState render_view_state;
	uint64_t next_render_frame_number = 0;

private:
	void init();
	void configure_ecs();
	void reset_scene_state();
	void shutdown_impl();
	void process_objs_to_delete();
	void publish_completed_render_frame();
	RenderFrame build_render_frame();
	RenderableDefinitionPtr get_renderable_definition(
		RenderableID id, const RenderableAttachment& attachment);
	RenderSkeletonDefinitionPtr get_render_skeleton_definition(
		SkeletonID id, const SkeletalRenderStateSnapshot& snapshot);
	void validate_renderable_resources(const Renderable& renderable) const;
	std::unique_ptr<Analytics> TPS_counter;
	float tps;
	bool paused = false;
	EGameMode game_mode = EGameMode::EDITOR;
	PlayerCharacter* active_player = nullptr;
	bool camera_orbit_with_right_mouse = false;
	bool free_camera_movement = false;
	bool normal_mode_cursor_captured = true;

public: // callbacks
	virtual void scroll_callback(double yoffset, bool gui_wants_input = false) override;
	virtual void key_callback(const KeyInput& key_input) override;
	virtual void mouse_button_callback(const MouseInput& mouse_input, bool gui_wants_input) override;
	// void pause();

private: // friends
	friend Experimental;
};
