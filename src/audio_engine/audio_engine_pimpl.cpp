#include "audio_engine_pimpl.hpp"

#include "audio_engine.hpp"

AudioEnginePimpl::AudioEnginePimpl(const AudioOutputMode mode) :
	audio_engine(std::make_unique<AudioEngine>(mode == AudioOutputMode::DEFAULT))
{
}

AudioEnginePimpl::~AudioEnginePimpl() = default;

AudioSource AudioEnginePimpl::create_source()
{
	return AudioSource(*audio_engine);
}

bool AudioEnginePimpl::has_output() const
{
	return audio_engine->has_output();
}

void AudioEnginePimpl::set_listener_transform(
	const glm::vec3& position,
	const glm::vec3& direction,
	const glm::vec3& up)
{
	audio_engine->set_listener_transform(position, direction, up);
}
