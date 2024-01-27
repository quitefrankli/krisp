#pragma once

#include "maths.hpp"
#include "identifications.hpp"

#include <map>
#include <string>
#include <functional>
#include <vector>
#include <filesystem>
#include <optional>


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

template<typename GameEngineT>
class GuiWindow
{
public:
	// used in graphics engine
	virtual void draw() = 0;
	virtual ~GuiWindow() = default;

	// used in game engine
	virtual void process(GameEngineT&) {}

	GuiWindow() = default;
	GuiWindow(GuiWindow&&) noexcept = default;
	GuiWindow(const GuiWindow&) = delete;
};

template<typename GameEngineT>
class GuiGraphicsSettings : public GuiWindow<GameEngineT>
{
public:
	GuiGraphicsSettings();

	virtual void draw() override;
	virtual void process(GameEngineT& engine) override;

public:
	float light_strength = 1.0f;
	GuiVar<bool> rtx_on = false;
	GuiVar<int> selected_camera_projection = 0;
	GuiVar<bool> wireframe_mode = false;

private:
	const std::vector<const char*> camera_projections = { "perspective", "orthographic" };
};

template<typename GameEngineT>
class GuiObjectSpawner : public GuiWindow<GameEngineT>
{
public:
	GuiObjectSpawner();

	virtual void process(GameEngineT& engine) override;
	virtual void draw() override;

private:
	using spawning_function_type = std::function<void(GameEngineT&)>;
	std::map<std::string, spawning_function_type> mapping;
	spawning_function_type* spawning_function = nullptr;

	const float button_width = 120.0f;
	const float button_height = 20.0f;
};

class ImFont;
template<typename GameEngineT>
class GuiFPSCounter : public GuiWindow<GameEngineT>
{
public:
	virtual void process(GameEngineT& engine) override;
	virtual void draw() override;

private:
	ImFont* font = nullptr;
	float fps = 0.0f;
	float tps = 0.0f;
	std::optional<uint32_t> window_width;
};

class AudioSource;

template<typename GameEngineT>
class GuiMusic : public GuiWindow<GameEngineT>
{
public:
	GuiMusic(AudioSource&& audio_source);
	virtual ~GuiMusic() override;
	virtual void process(GameEngineT& engine) override;
	virtual void draw() override;

private:
	std::unique_ptr<AudioSource> audio_source;
	float gain = 1.0f;
	float pitch = 1.0f;
	glm::vec3 position{};
	int selected_song = 0;
	std::vector<const char*> songs;
	// this is just for memory management for the above
	std::vector<std::string> songs_;
	std::vector<std::filesystem::path> songs_paths;
};

template<typename GameEngineT>
class GuiStatistics : public GuiWindow<GameEngineT>
{
public:
	virtual void process(GameEngineT& engine) override;
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
template<typename GameEngineT>
class GuiDebug : public GuiWindow<GameEngineT>
{
public:
	virtual void process(GameEngineT& engine) override;
	virtual void draw() override;

private:
	bool should_refresh_objects_list = false;
	std::vector<ObjectID> object_ids;
	std::vector<std::string> object_ids_strs;
	GuiVar<ObjectID> selected_object = ObjectID(0);
	GuiVar<bool> show_bone_visualisers = false; 
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

template<typename GameEngineT>
class GuiPhoto : public GuiWindow<GameEngineT>, public GuiPhotoBase
{
public:
	GuiPhoto();

	void init(std::function<void(const std::string_view)>&& texture_requester);

	virtual void process(GameEngineT& engine) override;
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
template<typename GameEngineT>
class GuiRenderSlicer : public GuiWindow<GameEngineT>, public GuiPhotoBase
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