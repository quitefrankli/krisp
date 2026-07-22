#include "audio_source.hpp"

#include "audio_engine.hpp"

#include <miniaudio.h>

#include <stdexcept>
#include <string>

namespace
{
void require_success(const char* operation, const ma_result result)
{
	if (result != MA_SUCCESS)
		throw std::runtime_error(std::string(operation) + ": " + ma_result_description(result));
}
}

class AudioSource::Impl
{
public:
	explicit Impl(AudioEngine& engine) : engine(engine) {}
	~Impl()
	{
		if (sound)
			ma_sound_uninit(sound.get());
	}

	AudioEngine& engine;
	std::unique_ptr<ma_sound> sound;
	float gain = 1.0f;
	float pitch = 1.0f;
	glm::vec3 position{};
	bool loop = false;
};

AudioSource::AudioSource(AudioEngine& audio_engine) :
	impl(std::make_unique<Impl>(audio_engine))
{
}

AudioSource::AudioSource(AudioSource&& other) noexcept = default;
AudioSource& AudioSource::operator=(AudioSource&& other) noexcept = default;
AudioSource::~AudioSource() = default;

void AudioSource::set_audio(const std::string_view filename, const AudioLoadMode mode)
{
	const std::string path(filename);
	auto replacement = std::make_unique<ma_sound>();
	const ma_uint32 flags = mode == AudioLoadMode::STREAM
		? MA_SOUND_FLAG_STREAM : MA_SOUND_FLAG_DECODE;
	require_success("Failed to load audio",
		ma_sound_init_from_file(&impl->engine.native(), path.c_str(), flags, nullptr, nullptr, replacement.get()));
	ma_sound_set_volume(replacement.get(), impl->gain);
	ma_sound_set_pitch(replacement.get(), impl->pitch);
	ma_sound_set_position(replacement.get(), impl->position.x, impl->position.y, impl->position.z);
	ma_sound_set_looping(replacement.get(), impl->loop ? MA_TRUE : MA_FALSE);
	if (impl->sound)
		ma_sound_uninit(impl->sound.get());
	impl->sound = std::move(replacement);
}

void AudioSource::play()
{
	if (!impl->sound)
		throw std::runtime_error("Cannot play audio before loading a file");
	require_success("Failed to rewind audio", ma_sound_seek_to_pcm_frame(impl->sound.get(), 0));
	require_success("Failed to play audio", ma_sound_start(impl->sound.get()));
}

void AudioSource::stop()
{
	if (impl->sound)
		require_success("Failed to stop audio", ma_sound_stop(impl->sound.get()));
}

void AudioSource::set_gain(const float gain)
{
	impl->gain = gain;
	if (impl->sound)
		ma_sound_set_volume(impl->sound.get(), gain);
}

void AudioSource::set_pitch(const float pitch)
{
	impl->pitch = pitch;
	if (impl->sound)
		ma_sound_set_pitch(impl->sound.get(), pitch);
}

void AudioSource::set_position(const glm::vec3& position)
{
	impl->position = position;
	if (impl->sound)
		ma_sound_set_position(impl->sound.get(), position.x, position.y, position.z);
}

void AudioSource::set_loop(const bool loop)
{
	impl->loop = loop;
	if (impl->sound)
		ma_sound_set_looping(impl->sound.get(), loop ? MA_TRUE : MA_FALSE);
}
