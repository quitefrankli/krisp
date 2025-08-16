#pragma once

#include <glm/vec3.hpp>

#include <cstdint>
#include <string_view>


class AudioEngine;

// class AudioSource
// {
// public:
// 	AudioSource(AudioEngine& audio_engine);
// 	AudioSource(AudioSource&& other) noexcept;
// 	AudioSource(const AudioSource&) = delete;
// 	~AudioSource();

// 	void set_audio(const std::string_view filename);
// 	void play();
// 	void stop();

// public: // getters and setters
// 	void set_gain(float gain);
// 	void set_pitch(float pitch);
// 	void set_position(const glm::vec3& position);
// 	void set_loop(bool loop);

// private:
// 	void set_audio_buffer(const uint32_t buffer);
// 	uint32_t p_Source;
// 	float p_Pitch = 1.0f;
// 	float p_Gain = 1.0f;
// 	glm::vec3 position{};
// 	glm::vec3 velocity{};
// 	bool p_LoopSound = false;
// 	uint32_t p_Buffer = 0;
// 	AudioEngine& audio_engine;
// 	bool should_destroy = true;
// };

class AudioSource
{
public:
	AudioSource(AudioEngine& audio_engine){}
	AudioSource(AudioSource&& other) noexcept{}
	AudioSource(const AudioSource&) = delete;
	~AudioSource(){}

	void set_audio(const std::string_view filename){}
	void play(){}
	void stop(){}

public: // getters and setters
	void set_gain(float gain){}
	void set_pitch(float pitch){}
	void set_position(const glm::vec3& position){}
	void set_loop(bool loop){}
};