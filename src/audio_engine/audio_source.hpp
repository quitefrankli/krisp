#pragma once

#include <glm/vec3.hpp>

#include <memory>
#include <string_view>

class AudioEngine;

enum class AudioLoadMode
{
	DECODE,
	STREAM
};

class AudioSource
{
public:
	explicit AudioSource(AudioEngine& audio_engine);
	AudioSource(AudioSource&& other) noexcept;
	AudioSource& operator=(AudioSource&& other) noexcept;
	AudioSource(const AudioSource&) = delete;
	AudioSource& operator=(const AudioSource&) = delete;
	~AudioSource();

	void set_audio(std::string_view filename, AudioLoadMode mode = AudioLoadMode::DECODE);
	void play();
	void stop();
	void set_gain(float gain);
	void set_pitch(float pitch);
	void set_position(const glm::vec3& position);
	void set_loop(bool loop);

private:
	class Impl;
	std::unique_ptr<Impl> impl;
};
