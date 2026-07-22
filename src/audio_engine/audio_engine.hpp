#pragma once

#include <miniaudio.h>

#include <glm/vec3.hpp>

class AudioEngine
{
public:
	explicit AudioEngine(bool enable_output);
	~AudioEngine();

	AudioEngine(const AudioEngine&) = delete;
	AudioEngine& operator=(const AudioEngine&) = delete;

	bool has_output() const { return output_available; }
	ma_engine& native() { return engine; }
	void set_listener_transform(
		const glm::vec3& position,
		const glm::vec3& direction,
		const glm::vec3& up);

private:
	ma_engine engine{};
	bool initialized = false;
	bool output_available = false;
};
