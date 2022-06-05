#pragma once

#include "maths.hpp"

#include <map>
#include <string>
#include <functional>
#include <vector>


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

public:
	float light_strength = 1.0f;
	Maths::Ray light_ray;	
};

template<typename GameEngineT>
class GuiObjectSpawner : public GuiWindow<GameEngineT>
{
public:
	GuiObjectSpawner();

	virtual void process(GameEngineT& engine) override;
	virtual void draw() override;

public:
	bool use_texture = false;

private:
	using spawning_function_type = std::function<void(GameEngineT&, bool)>;
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
	GuiFPSCounter(unsigned initial_window_width);

	virtual void draw() override;

	float fps = 0.0f;
	float tps = 0.0f;
	unsigned window_width;

private:
	ImFont* font = nullptr;
};

class AudioSource;
namespace std {
	namespace filesystem {
		class path;
	}
}

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
	glm::vec3 position;
	int selected_song = 0;
	std::vector<const char*> songs;
	// this is just for memory management for the above
	std::vector<std::string> songs_;
	std::vector<std::filesystem::path> songs_paths;
};