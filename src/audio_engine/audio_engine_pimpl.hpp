#pragma once

#include "audio_source.hpp"

#include <glm/vec3.hpp>

#include <memory>

class AudioEngine;

enum class AudioOutputMode
{
	DEFAULT,
	NO_DEVICE
};

class AudioEnginePimpl
{
public:
	explicit AudioEnginePimpl(AudioOutputMode mode = AudioOutputMode::DEFAULT);
	~AudioEnginePimpl();

	AudioSource create_source();
	bool has_output() const;
	void set_listener_transform(
		const glm::vec3& position,
		const glm::vec3& direction,
		const glm::vec3& up);

private:
	std::unique_ptr<AudioEngine> audio_engine;
};
