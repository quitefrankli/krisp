#pragma once

#include "maths.hpp"
#include "identifications.hpp"
#include "renderable/render_types.hpp"
#include "save_file_store.hpp"
#include "gui_window_helpers.hpp"

#include <map>
#include <string>
#include <functional>
#include <vector>
#include <filesystem>
#include <optional>
#include <utility>
#include <array>


class GameEngine;
struct KeyInput;

enum class GuiPanelDock
{
	LEFT,
	RIGHT,
	BOTTOM,
	NONE
};

struct GuiPanelInfo
{
	std::string id;
	std::string title;
	GuiPanelDock default_dock = GuiPanelDock::NONE;
	bool initially_visible = true;
	bool dockable = true;
};

template<typename T>
struct GuiVar
{
	bool changed = false;
	T value;

	GuiVar() = default;
	GuiVar(const T& value) : value(value)
	{
	}

	GuiVar& operator=(const T& value)
	{
		this->value = value;
		return *this;
	}

	operator T() const
	{
		return value;
	}
};

class GuiWindow
{
public:
	// used in graphics engine
	virtual void draw() = 0;
	virtual ~GuiWindow() = default;

	// used in game engine
	virtual void process(GameEngine&) {}

	explicit GuiWindow(GuiPanelInfo panel = {}) :
		panel(std::move(panel)),
		imgui_name(this->panel.title + "###" + this->panel.id),
		visible(this->panel.initially_visible)
	{
	}
	GuiWindow(GuiWindow&&) noexcept = default;
	GuiWindow(const GuiWindow&) = delete;

	const GuiPanelInfo& get_panel_info() const { return panel; }
	const char* get_imgui_name() const { return imgui_name.c_str(); }
	bool* get_visible_ptr() { return &visible; }
	bool is_visible() const { return visible; }
	void set_visible(bool value);
	void restore_visibility(bool value) { visible = value; }
	void reset_visibility() { set_visible(panel.initially_visible); }

protected:
	bool begin(int flags = 0, bool closable = true);
	void end();

private:
	GuiPanelInfo panel;
	std::string imgui_name;
	bool visible = true;
};

class EngineUiWindow : public GuiWindow
{
public:
	using GuiWindow::GuiWindow;
};

class GuiGraphicsSettings : public EngineUiWindow
{
public:
	GuiGraphicsSettings();

	virtual void draw() override;
	virtual void process(GameEngine& engine) override;

public:
	float light_strength = 1.0f;
	GuiVar<int> selected_camera_projection = 0;
	GuiVar<ERenderMode> render_mode = ERenderMode::RASTERIZED;
	GuiVar<float> exposure_ev = 0.0f;

	ERenderMode get_render_mode() const { return render_mode.value; }
	bool select_render_mode(ERenderMode mode);

private:
	const std::vector<const char*> camera_projections = { "perspective", "orthographic" };
};

class GuiSaveManager : public EngineUiWindow
{
public:
	GuiSaveManager();
	void draw() override;
	void process(GameEngine& engine) override;

private:
	enum class Action { SAVE, LOAD, DELETE_SAVE };
	struct Request { Action action; std::string name; };

	void queue(Action action, const std::string& name);

	SaveFileStore store;
	std::vector<SaveFileEntry> entries;
	std::optional<std::string> selected;
	std::optional<Request> pending;
	std::string status;
	bool status_is_error = false;
	bool refresh_requested = true;
	std::array<char, 128> name_buffer{};
};

class GuiObjectSpawner : public EngineUiWindow
{
public:
	GuiObjectSpawner();

	virtual void process(GameEngine& engine) override;
	virtual void draw() override;

private:
	using spawning_function_type = std::function<void(GameEngine&)>;
	std::map<std::string, spawning_function_type> mapping;
	spawning_function_type* spawning_function = nullptr;
	std::optional<std::string> load_error;

	const float button_width = 120.0f;
	const float button_height = 20.0f;
};

class AudioSource;

class GuiMusic : public EngineUiWindow
{
public:
	GuiMusic(AudioSource&& audio_source);
	virtual ~GuiMusic() override;
	virtual void process(GameEngine& engine) override;
	virtual void draw() override;
	static std::optional<std::string> selected_path(
		const std::vector<std::string>& paths,
		int selected_index);
	static std::vector<std::string> sort_paths(std::vector<std::string> paths);

private:
	std::unique_ptr<AudioSource> audio_source;
	float gain = 1.0f;
	float pitch = 1.0f;
	glm::vec3 position{};
	bool loop = false;
	GuiVar<int> selected_song = 0;
	std::vector<std::string> songs_paths;
	GuiWindowDetail::ResourceTree songs_tree;
	std::optional<std::string> audio_to_play;
	std::optional<std::string> load_error;
};

class GuiStatistics : public EngineUiWindow
{
public:
	GuiStatistics();
	virtual void process(GameEngine& engine) override;
	virtual void draw() override;

	void update_buffer_capacities(const std::vector<std::pair<size_t, size_t>>& buffer_capacities);

private:
	struct BufferCapacity
	{
		size_t total_capacity = 0;
		size_t filled_capacity = 0;
	};

	std::vector<std::pair<std::string, BufferCapacity>> buffer_capacities = {
		{ "vertex buffer", {} },
		{ "index buffer", {} },
		{ "uniform buffer", {} },
		{ "materials buffer", {} },
		// For ray tracing:
		// { "mapping buffer", {} },
		{ "bone buffer", {} }
	};
};

class GuiPhotoBase
{
public:
	void update(void* img_rsrc, const glm::uvec2& true_img_size, uint32_t requested_width = 0);

	uint32_t get_requested_width() const { return requested_width; }
	uint32_t get_requested_height() const { return static_cast<uint32_t>(static_cast<float>(requested_width) / get_aspect_ratio()); }

protected:
	// width/height
	float get_aspect_ratio() const { return float(true_dims.x) / float(true_dims.y); }
	// IMPORTANT, call this between ImGui::Begin and ImGui::End
	void draw();

	void* img_rsrc = nullptr; // for vulkan this is a VkDescriptorSet
	glm::uvec2 true_dims;
	int requested_width = 300;
};

class GuiPhoto : public EngineUiWindow, public GuiPhotoBase
{
public:
	GuiPhoto();

	void init(std::function<void(std::string_view)>&& texture_requester);

	virtual void process(GameEngine& engine) override;
	virtual void draw() override;

private:
	void refresh_textures();

	std::vector<std::string> photo_paths;
	GuiWindowDetail::ResourceTree photo_tree;
	bool should_show = false;
	bool should_refresh_textures = false;
	bool texture_dropdown_open = false;
	GuiVar<int> selected_image = 0;
	std::optional<std::string> texture_to_show;
	std::function<void(std::string_view)> texture_requester;
	std::optional<std::string> load_error;
};

// Shows mid-render slices for visualisation/debug purposes
// i.e. shadow map
// Currently the output of this window can feel laggy, but that's because we are (for simplicity) only using a single frame in the swapchain
// can easily be improved by using all frames, however unnecessary for now since it's only used for debugging
class GuiRenderSlicer : public EngineUiWindow, public GuiPhotoBase
{
public:
	GuiRenderSlicer();
	using requester_t = std::function<void(const std::string&)>;

	virtual void draw() override;

	void init(const requester_t& slice_requester) { this->slice_requester = slice_requester; }

private:
	std::vector<std::string> render_slices = { "none", "shadow_map" };

	GuiVar<int> selected_slice = 0;
	// it can take the quad renderer some time to catchup after a transition is requested
	int cycles_before_draw = 0;
	requester_t slice_requester;
};
