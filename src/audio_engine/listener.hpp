#pragma once

#include <glm/vec3.hpp>

class AudioEnginePimpl;

class Listener
{
public:
	Listener() = default;
	explicit Listener(AudioEnginePimpl& audio_engine) : audio_engine(&audio_engine) {}

	void set_transform(
		const glm::vec3& position,
		const glm::vec3& direction,
		const glm::vec3& up);

private:
	AudioEnginePimpl* audio_engine = nullptr;
};
