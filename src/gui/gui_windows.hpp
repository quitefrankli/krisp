#pragma once

#include "maths.hpp"
#include "identifications.hpp"

#include <map>
#include <string>
#include <functional>
#include <vector>
#include <filesystem>
#include <optional>


class GameEngine;

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

	GuiWindow() = default;
	GuiWindow(GuiWindow&&) noexcept = default;
	GuiWindow(const GuiWindow&) = delete;
};

class GuiGraphicsSettings : public GuiWindow
{
public:
	GuiGraphicsSettings();

	virtual void draw() override;
	virtual void process(GameEngine& engine) override;

public:
	float light_strength = 1.0f;
	GuiVar<bool> rtx_on = false;
	GuiVar<int> selected_camera_projection = 0;
	GuiVar<bool> wireframe_mode = false;

private:
	const std::vector<const char*> camera_projections = { "perspective", "orthographic" };
};

class GuiObjectSpawner : public GuiWindow
{
public:
	GuiObjectSpawner();

	virtual void process(GameEngine& engine) override;
	virtual void draw() override;

private:
	using spawning_function_type = std::function<void(GameEngine&)>;
	std::map<std::string, spawning_function_type> mapping;
	spawning_function_type* spawning_function = nullptr;

	const float button_width = 120.0f;
	const float button_height = 20.0f;
};

class GuiModelSpawner : public GuiWindow
{
public:
	GuiModelSpawner();

	virtual void process(GameEngine& engine) override;
	virtual void draw() override;

private:
	std::vector<std::string> models;
	std::vector<std::filesystem::path> model_paths;
	GuiVar<int> selected_model = 0;
	bool should_spawn = false;
};

class ImFont;
class GuiFPSCounter : public GuiWindow
{
public:
	virtual void process(GameEngine& engine) override;
	virtual void draw() override;

private:
	ImFont* font = nullptr;
	float fps = 0.0f;
	float tps = 0.0f;
	std::optional<uint32_t> window_width;
};

class AudioSource;

class GuiMusic : public GuiWindow
{
public:
	GuiMusic(AudioSource&& audio_source);
	virtual ~GuiMusic() override;
	virtual void process(GameEngine& engine) override;
	virtual void draw() override;

private:
	std::unique_ptr<AudioSource> audio_source;
	float gain = 1.0f;
	float pitch = 1.0f;
	glm::vec3 position{};
	GuiVar<int> selected_song = 0;
	std::vector<std::string> songs;
	std::vector<std::filesystem::path> songs_paths;
};

class GuiStatistics : public GuiWindow
{
public:
	virtual void process(GameEngine& engine) override;
	virtual void draw() override;

	void update_buffer_capacities(const std::vector<std::pair<size_t, size_t>>& buffer_capacities);

private:
	struct BufferCapacity
	{
		size_t total_capacity = 0;
		size_t filled_capacity = 0;
	};

	BufferCapacity vertex_buffer_capacity;
	BufferCapacity index_buffer_capacity;
	BufferCapacity uniform_buffer_capacity;
	BufferCapacity materials_buffer_capacity;
	BufferCapacity mapping_buffer_capacity;
	BufferCapacity bone_buffer_capacity;
};

class Object;
class GuiDebug : public GuiWindow
{
public:
	virtual void process(GameEngine& engine) override;
	virtual void draw() override;

private:
	bool should_refresh_objects_list = false;
	bool should_toggle_pause = false;
	bool is_paused = false;
	std::vector<ObjectID> object_ids;
	std::vector<std::string> object_ids_strs;
	GuiVar<ObjectID> selected_object = ObjectID(0);
	GuiVar<bool> show_bone_visualisers = false;
	std::string filter_text = std::string(1024, '\0');
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

class GuiPhoto : public GuiWindow, public GuiPhotoBase
{
public:
	GuiPhoto();

	void init(std::function<void(const std::string_view)>&& texture_requester);

	virtual void process(GameEngine& engine) override;
	virtual void draw() override;

private:
	std::vector<std::string> photos;
	std::vector<std::filesystem::path> photo_paths;
	bool should_show = false;
	GuiVar<int> selected_image = 0;
	std::function<void(const std::string_view)> texture_requester;
};

// Shows mid-render slices for visualisation/debug purposes
// i.e. shadow map
// Currently the output of this window can feel laggy, but that's because we are (for simplicity) only using a single frame in the swapchain
// can easily be improved by using all frames, however unnecessary for now since it's only used for debugging
class GuiRenderSlicer : public GuiWindow, public GuiPhotoBase
{
public:
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

class GuiAnimationSelector : public GuiWindow
{
public:
	virtual void process(GameEngine& engine) override;
	virtual void draw() override;

private:
	std::optional<AnimationID> selected_animation;
	std::string selected_animation_name = "";
	bool loop = false;
	bool should_play = false;
};
